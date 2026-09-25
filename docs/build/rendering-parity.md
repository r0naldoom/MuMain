# Rendering parity

The client uses one SDL GPU rendering policy on every desktop platform. SDL
selects the native GPU driver; game rendering code does not select a backend or
change performance behavior by OS.

| OS | SDL GPU driver | Application policy |
|----|----------------|--------------------|
| macOS | Metal | Identical |
| Linux | Vulkan | Identical |
| Windows | D3D12 | Identical |

Native drivers, GPUs, and font rasterizers can still expose platform-specific
driver defects. Runtime parity therefore requires measurements on each target;
a successful build or a macOS run is not evidence for Linux or Windows.

## Build policy

- Debug builds enable SDL GPU validation.
- Builds with `NDEBUG` disable SDL GPU validation.
- The policy is fixed in code. There is no OS branch or runtime override.
- GPU skinning shaders and pipelines are required. An eligible submission
  failure is counted and rejected; it does not silently switch to CPU skinning.

## Packaged font roles

Release builds resolve every text role from files beside the executable:

| Role | Family/file |
|------|-------------|
| Normal | Selected family regular file |
| Bold | Selected family bold file |
| Big bold | Selected family bold file |
| Fixed | `fonts/Cousine-Regular.ttf` |
| Missing glyph fallback | `fonts/NanumGothic-Regular.ttf` |

Selectable families are DejaVu Sans and Liberation Sans. Empty or unknown
configuration selects DejaVu Sans. SDL_ttf, Windows GDI, and the non-Windows
GDI shim use the same registry.

SDL_ttf attaches Nanum Gothic to every role at startup. This preserves the
selected Latin family while rendering Hangul labels instead of missing-glyph
boxes; bold roles use SDL_ttf's synthetic bold style for the fallback face.

Missing or corrupt packaged roles abort Release renderer startup. Windows also
requires private GDI registration of every packaged role; partial registration
is rolled back.

Debug builds may use system fonts only when a packaged role is unavailable.
Logs mark this path as `NON-PARITY developer font fallback`. This is the sole
documented platform-dependent font exception. Release packages may not use it.

## SDL_ttf patch policy

SDL_ttf is pinned to revision
`a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b` from the 3.2.2 line. Upstream
`main` was checked at `a42434b8c96daaf7650dbd0befe480c090d1c2eb`; it did
not contain equivalent missing-glyph upload batching.

The local platform-neutral patch
`cmake/patches/sdl_ttf-3.2.2-batch-glyph-uploads.patch` batches one text
update's missing glyphs into one copy pass and command submission. It exposes
the upload count through the custom text property
`MuMain.SDL_ttf.gpu_text.uploaded_glyphs`. The client consumes that property
once and reports it as `GlyphUploads`.

## Capture procedure

Use Release builds for platform comparisons. Keep these inputs identical:

- revision;
- account and character;
- scene and visible objects;
- camera position and direction;
- resolution and window mode;
- VSync and frame-limit settings;
- editor enabled/disabled state;
- warm-up duration.

On Linux, run from the packaged runtime directory:

```bash
MU_RENDER_TIMING=1 ./Main
```

On macOS, use:

```bash
MU_RENDER_TIMING=1 ./Main.app/Contents/MacOS/Main
```

On Windows PowerShell, use:

```powershell
$env:MU_RENDER_TIMING = "1"
.\Main.exe
```

To isolate D3D12 non-indexed triangle batching, rerun with:

```powershell
$env:MU_D3D12_DISABLE_TRIANGLE_MERGING = "1"
.\Main.exe
```

### RenderDoc semantic object-pass label

The static-object completion label is disabled by default. Enable it only for a
RenderDoc capture:

```bash
MU_RENDERDOC_LABELS=1 renderdoccmd capture --wait-for-exit --capture-file scene-anchor ./Main
```

The enabled label is `mu.scene.static-objects.complete`. It marks the command
stream immediately after the initial static-object pass and before character
rendering. Verify a capture with:

```bash
rdc --session scene-anchor open scene-anchor.rdc
rdc --session scene-anchor events --filter 'mu.scene.static-objects.complete' --json
rdc --session scene-anchor close
```

The label itself has no render target. A frame comparator resolves the preceding
rendered event on the same target. Without `MU_RENDERDOC_LABELS`, the client
does not enqueue a label command or call the GPU debug-label API.

### Lost Tower wall fixture

The current SDL GPU client accepts a reproducible static-object fixture:

```bash
ulimit -n 65536
MU_RENDERDOC_LABELS=1 renderdoccmd capture --wait-for-exit --capture-file lost-tower-wall \
  ./Main --scene=lost-tower-wall-v1 --capture-when-ready --exit-after-capture
```

The selector is parsed from `lpszCommandLine`: Windows supplies that command
line and the Linux entry point forwards `argv` into it. The historical a410
Linux client did not forward `argv`; keep its compatibility patch local and
version it with the comparison fixture rather than changing its tracked
history.

`lost-tower-wall-v1` pins the current client to a windowed `800×600` target,
Lost Tower map `4`, and grid position `(213, 74)`. The position lies inside the
Lost Tower safe zone, where the client terrain attribute `0x01` covers x
`198..213` and y `70..75`; monsters neither enter nor attack there, which keeps
combat lighting out of the scene (see "Dynamic light reaches the anchor"
below). The south wall (y `69`) and the east wall (x `214..215`) meet at
`(214, 69)`, next to the position, so the Default camera frames that wall
corner instead of open floor. The tile itself was chosen by driving a
client over the developer control socket and reading `state` and `screenshot`
at every candidate inside the safe zone: at `(213, 74)` the wall runs
diagonally across the frame with the crystal-capped pillars in view, and a
36-second sample kept a single monster in `nearby`, wandering 9 to 11 tiles
away. `(213, 72)` sits on a wander path and caught a monster in melee range
with its aura in frame; `(211, 71)` and `(208, 72)` report five monsters and
frame mostly floor. A monster inside the safe zone cannot attack, but its
effects still light the scene, so require `nearby` to hold no monster within 8
tiles before a capture and retry when it does. The fixture applies its
target after loading `config.ini` and before SDL creates the window, so a saved
fullscreen or display resolution cannot alter a capture. Its world viewport
reserves the legacy `48` reference pixels rather than the gameplay HUD's `51`;
this fixture-only override removes a known geometric delta from cross-build
material comparisons and is not a judgment on the new HUD design. The
historical client already reserves `48` reference pixels, and its local
comparison patch MUST apply the same windowed target.

Place the dedicated fixture character on map `4`, grid `(213, 74)`, before a
parity run. Run the placement update while the client is closed: it otherwise
saves its in-memory position on logout and overwrites the database value. The
following SQL is a template; substitute the dedicated character name and keep
credentials and local paths out of tracked documentation:

```sql
UPDATE data."Character" SET "PositionX" = 213, "PositionY" = 74
WHERE "Name" = '<fixture-character>';
SELECT "Name", "PositionX", "PositionY" FROM data."Character"
WHERE "Name" = '<fixture-character>';
```

Because version 1 does not set camera state in-process, verify before each
capture pair that both builds have the same `[Camera] Zoom` value in
`config.ini`. The reference pair uses `Zoom=1735`; recheck it after any manual
zoom or configuration change and record the value with the capture result.

The fixture becomes ready only after the server's join or revival packet
reports exactly that map and position. A mismatch emits `[SceneFixture] ... is
not ready`; readiness gates only the automatic capture scheduler, so a capture
cannot be reported as fixture-ready accidentally.

While selected, the fixture freezes the clock around the main-scene update,
where Lost Tower derives static-object texture coordinates. It restores
wall-clock time before reconnect/network work, then freezes the render clock
again. Version 1 deliberately does not override the camera or static-object
luminosity: reproducible camera state comes from the positioned, stationary
character. The legacy comparison patch drops the unused `rand()` brightness
value from the Lost Tower static-object branch, as #609 did in the current
client. Both clients seed `rand()` from wall-clock time at startup, so random
sequences differ between launches regardless of how many values either build
consumes; aligning random consumption cannot make a capture reproducible.

#### Dynamic light reaches the anchor

The `mu.scene.static-objects.complete` anchor precedes the character and effect
passes, so monsters, skills, and effects are not drawn into the anchored render
target. Their light is. Effects add dynamic terrain light while they move, before
the scene renders, and that light tints both the terrain and every static object
standing in it. A monster attack near the fixture can therefore turn part of the
wall orange at the anchor while no monster pixel appears there.

Two legacy-client captures taken at grid `(91, 183)`, outside the safe zone,
differed in 8.4% of the wall region. In the later launch monsters had reached
the character and a combat effect above one wall section was lighting it; the
earlier launch had neither. The affected pixels came from the same draws at the
same depths, with red and green raised and blue unchanged.

Before accepting a capture, export its final render target and confirm that no
monster, skill, or other effect is active near the measured region.

`--capture-when-ready` is opt-in; without it, the fixture leaves RenderDoc's
manual F12 capture unchanged. With it, the fixture waits 240 rendered frames
following readiness before calling RenderDoc's in-process trigger. Record
`warmup_frames = 240` in the comparison manifest. `--exit-after-capture`
posts the normal clean-shutdown event two completed frames after that capture,
so `renderdoccmd --wait-for-exit` can finalize the `.rdc`. RenderDoc captures
the frame after the trigger call. If the client was not launched under RenderDoc,
the automatic trigger emits one diagnostic line, does nothing, and does not
request shutdown.

#### Independent-capture check

Capture two separate client launches; do not reuse a process or substitute a
numeric RenderDoc event ID:

```bash
ulimit -n 65536
MU_RENDERDOC_LABELS=1 renderdoccmd capture --wait-for-exit --capture-file fixture-run-a \
  ./Main --scene=lost-tower-wall-v1 --capture-when-ready --exit-after-capture
MU_RENDERDOC_LABELS=1 renderdoccmd capture --wait-for-exit --capture-file fixture-run-b \
  ./Main --scene=lost-tower-wall-v1 --capture-when-ready --exit-after-capture
```

After logging in once, let each client observe the matching server spawn. The
fixture then captures and exits automatically. In the comparator manifest,
configure both render targets with:

```toml
anchor="mu.scene.static-objects.complete"
color_target=0
```

The comparator resolves the rendered event immediately preceding that anchor.
Record the capture hashes, revision, data and font identifiers, backend, GPU,
driver, window mode, VSync, frame limit, and display size with the result.

The fixture proves reproducibility of the anchored static-object render target
between independent launches under the same executable, data, fonts, machine,
GPU driver, backend, resolution, and runtime settings. It does not prove
parity between operating systems, GPU drivers, GPUs, resolutions, or a final
frame after character and UI passes. Measure those targets separately.

Then:

1. Enable `$glstats on`.
2. Load the agreed scene and camera.
3. Warm recurring labels and visible assets until steady state.
4. Confirm validation state and bundled font paths in the log.
5. Capture 300 consecutive frames. The 60-frame diagnostics provide five
   samples.
6. Record requested/submitted draws, actual binds and uniform pushes, 2D
   merges, skinning paths, glyph uploads, replay time, submit time, and total
   frame time.

Recurring labels should report zero glyph uploads after warm-up. Failed GPU
skinning should remain zero. Submitted 2D draws and actual state applications
should be lower than requested draws in scenes with adjacent compatible work.

`$glstats` displays:

```text
Bind Pipe:<pipeline> Samp:<sampler> VU:<vertex> FU:<fragment>
2D Merge:<count> Glyph upload:<count>
Skin GPU:<submitted> CPU-ineligible:<cpu> Failed:<failed>
```
