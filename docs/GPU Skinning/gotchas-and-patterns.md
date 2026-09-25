# GPU Rendering Gotchas and Invariants

## Bone palettes

- A palette is a contiguous span of row-major 3x4 affine matrices: exactly 12
  floats per bone. `RenderSkinnedTriangles()` rejects empty or misaligned spans.
- Position and normal bone indices are separate. Reusing the position index for
  normals changes lighting on models whose source data distinguishes them.
- Pointer-only palette deduplication is unsafe. Stack-local arrays may reuse an
  address with different contents. Cache identity therefore includes both the
  palette pointer and `paletteVersion`.
- The per-frame bone storage buffer must grow before recording an offset. A
  failed growth or upload returns the draw to CPU fallback rather than recording
  a partial command.
- Uniform arrays and the nested skinning uniform block are value-initialized.
  The shader reads full 16-byte lanes even when C++ assigns only the used fields.

## Lazy CPU materialization

- `TransformCheap()` records animation state without eagerly transforming every
  vertex.
- `EnsureCpuVertices()` and `EnsureCpuNormals()` materialize data for fallback
  consumers. State such as body scale, bone scale, and translate mode must be
  captured when the deferred transform is requested, not read later from
  mutable globals.
- The shared global `::BoneTransform` must be materialized by `BMD::Transform()`
  before another `Animation()` can replace it. Debug builds assert on a deferred
  CPU consumer that observes the replacement; make that transform eager rather
  than copying a stale palette (#547).
- The guard was observed catching the defect it exists for, and the numbers are worth
  keeping because the log they came from is overwritten by the next run. With the
  eager-skin fix absent, a Magic Gladiator killing a monster in Lost Tower produced:

      [DXP-20] deferred shared BoneTransform replaced before skinning
               (BMD=0x7eaf9538e428, captured=1, current=2)

  `captured=1, current=2` is the whole mechanism in two integers: the object armed its
  deferral against generation 1 of the shared palette and something wrote generation 2
  before the skinning consumed it. Counters across two runs of the same scenario read
  `captures=59288 validations=483 divergences=0` before the fix and
  `captures=93627 validations=0 divergences=0` after it. The 483 matter as much as the
  divergence: they prove the validation seam is reachable rather than dead, and they go
  to zero afterwards because an eagerly skinned global palette leaves no deferred
  request to validate.

- Reproducing this in Debug needs the bounded bitmap diagnostic formatting fix
  (upstream #603). Without it the fortified `mu_swprintf` in `GlobalBitmap.cpp` aborts
  the client before login, and no scenario runs at all.

- Debug-only `$bonepalette` reports cumulative shared-palette captures,
  deferred-consumer validations, and detected divergences. With a confirmed bad
  frame, `captures == 0` means the capture seam is wrong; `captures > 0` with
  `validations == 0` means the path does not consume through `EnsureCpu*()`; and
  positive captures and validations with zero divergences means an uncounted
  writer or a replaced per-transform request is evading the guard.
- Cloth, shadow-volume, shadow-map, and vertex-wave paths intentionally remain
  CPU consumers. GPU eligibility must not bypass their materialization calls.

## Shader compatibility

- `SkinningTextureCoordinates` distinguishes mesh UVs, Chrome through Chrome7,
  Oil, and Metal. Do not collapse modes merely because legacy render flags share
  bits.
- Chrome UVs depend on animated wave, light, offset, and time values. A GPU path
  is equivalent only when those parameters are forwarded.
- `translate == false` means placement is already represented by the palette;
  the shader must not add body origin a second time.
- C++ uniform layout, HLSL declarations, and generated MSL/SPIR-V bindings form
  one contract. Keep size assertions and shader validation together.

## Deferred command lifetime

- Draw inputs must be copied into renderer-owned per-frame scratch storage before
  the caller's span expires.
- A `RenderCmd` snapshots pipeline, texture, sampler, offsets, fog, and vertex
  uniforms. Replay must not consult mutable draw state from a later call.
- Commands replay in submission order. Merging is allowed only for adjacent
  triangle commands with identical compatible state.
- `BeginFrame()` and `EndFrame()` bound all queued data. Submission outside an
  active frame is rejected.

## Batching and profiling

- Do not reorder blended terrain by texture pair. Base and overlay layers are
  separate downstream draws, and overlays may render with depth testing off.
  Only adjacent compatible commands may merge.
- A merge key must include pipeline, texture, sampler, MVP, fog, and contiguous
  vertex storage. Omitting captured state can replay earlier geometry with a
  later draw's settings.
- `FrameProfiler` is renderer-neutral. Do not add raw GL or SDL GPU query calls
  to it; backend statistics belong in `RendererStats`.
- `$glstats` reports CPU pass and SDL GPU submission timings. It does not claim
  per-pass GPU time.
- Reset profiler values after every overlay has consumed them, including frames
  where neither overlay is visible.

## Textures and resources

- Game bitmap IDs are not SDL GPU handles. Resolve them through the renderer's
  texture and sampler registries.
- Unknown textures, unavailable pipelines, or failed buffer growth skip the GPU
  draw safely. Never enqueue a command with incomplete resources.
- Frame readback is request-driven. Ordinary frames must not allocate transfer
  buffers or wait on readback fences.

## Compatibility boundary

- Historical GL-shaped helpers are wrappers, not permission for gameplay code
  to call raw graphics APIs.
- `tools/check_gl_wrapper_monopoly.py` must remain green. New rendering behavior
  belongs in `IMuRenderer`, its SDL GPU backend, or an existing render-layer
  compatibility wrapper.

## Animation synchronization

- `AnimationTaskPool` workers may update bone transforms asynchronously. Callers
  must honor the established wait barrier before reading shared palettes.
- Render-frequency code must not advance fixed-step simulation once per rendered
  frame. Existing timing controls preserve animation and cloth cadence when FPS
  changes.

## Batch-break attribution

- `$glstats` reports SDL GPU triangle batch draws, vertices per batch, and break
  causes for texture, blend, depth, pipeline, uniform, matrix, intervening draw,
  or other barriers.
- Attribute breaks where deferred SDL GPU commands fail to merge. State-wrapper
  calls only update logical renderer state; adding flush hooks there would restore
  the retired OpenGL ownership model and double-count attempted changes.
- Do not count the first triangle command of a frame as a break. A break requires
  an existing triangle batch candidate that the next submission cannot join.
