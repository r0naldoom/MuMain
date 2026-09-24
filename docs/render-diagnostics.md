# Render Diagnostics

> **Status:** the `console` control and SDL GPU draw filter are implemented.

## Purpose

A visual artifact can be caused by any draw in a long frame. The golden-panel
investigation (#621) required exporting 13 render targets and manually bisecting
a captured draw stream. The implemented console control and proposed draw filter
make that investigation live and repeatable:

1. execute existing diagnostic console commands through the control socket; and
2. suppress geometric draws selected by a recorded-draw predicate.

Both controls belong to developer builds only, alongside the existing control
socket gate. They do not change player-build behavior, gameplay commands, or
render ordering when disabled.

## Constraints and terminology

- A **submitted draw ordinal** is a zero-based count of geometry that reaches
  the renderer's final replay helper in one frame. It resets every frame.
- A submitted draw ordinal is **not** a RenderDoc EID. RenderDoc also records
  passes, uploads, bindings, uniform pushes, blits, and other backend events.
- An ordinal is post-merge. One logical caller submission can join an earlier
  compatible recorded command and therefore not create its own submitted draw.
- The draw filter applies only to geometry. It must preserve viewport, scissor,
  resource upload, and pass state so skipping one draw cannot leak state into
  later draws.

## Console commands through the control socket

### Existing behavior

`ControlServer` processes requests on the main thread, once per frame, after
network packets and before scene input. It can dispatch up to 16 requests from
one connection in that frame. `say` currently validates text and sends it to
the server; it does not run the local console parser.

The in-game chat calls the global `CheckCommand` wrapper. That wrapper first
calls `CmuConsoleDebug::CheckCommand`, then also implements chat, UI, trade,
and macro behavior. The wrapper is intentionally not the socket interface:
using it would make a developer render command depend on scene/UI lifetime and
could consume or send ordinary chat unexpectedly.

### Interface

The control socket provides a distinct command rather than changing the meaning
of `say`:

```json
{"cmd":"console","text":"$effects sprites off"}
```

It calls only `CmuConsoleDebug::CheckCommand` and returns whether the parser
consumed the text. It never sends a public chat packet, opens a chat box, or
simulates keyboard input. Unknown text receives a local control-socket error
rather than being sent as chat.

The request boundary catches standard exceptions from the legacy parser. This
contains malformed numeric arguments from `$fps`, `$winmsg`, and future parser
branches without duplicating the parser's grammar. Debug-only commands requested
in Release return `not_allowed`.


### Release command surface

The parser has 26 unconditional command branches available in the normal
Release build:

- `$fpscounter on/off`, `$details on/off`, `$glstats on/off`, `$fps <n>`;
- `$vsync on/off`, `$winmsg <n>`;
- `$effects on/off`;
- `$effects sprites`, `particles`, `skillmodels`, `boids`, `wingshadow`,
  `joints`, and `wingextralayers`, each with `on/off`.

Thirteen additional branches are Debug-only: `$open`, `$close`, `$clear`,
`$type_test`, `$texture_info`, `$color_test`, `$mapatt on/off`, `$path on/off`,
`$bb on/off`, and `$bonepalette`. `$bonepalette` reports cumulative shared
bone-palette captures, deferred-consumer validations, and divergences for the
#547 guard. They remain unavailable in Release; the new socket command must
report that state instead of inventing a second implementation.

### Ordering

Socket and keyboard commands can alter the same state in the same frame. The
last executed command wins. This is acceptable for developer tooling only if
the response reports the accepted command; it is not a transactional command
queue. The command must not use the global chat wrapper, whose unrelated UI
commands have different lifetime requirements.

## Draw filtering

### Current renderer seam

The SDL GPU renderer records deferred `RenderCmd` values during the frame,
uploads buffers, then replays them in order. There are six geometry producers:
text triangles, 2D quads, world triangles, 3D quads, skinned triangles, and
quad strips. They use direct `s_renderCmds.push_back` calls; there is no single
enqueue function.

There is one useful geometry submission seam: `ReplayDrawCommand`. Both the
normal replay pass and editor offscreen capture replay call it. Filtering there
covers every effective SDL GPU geometric draw while avoiding six producer hooks.
It also deliberately observes post-merge commands.

### Control interface

`draw-filter` installs one immutable AND predicate for subsequent frame replay:

```json
{"cmd":"draw-filter","texture_id":12778,"texture_width":16,"texture_height":16,"blend":false}
```

Every supplied clause must match: `texture_id`, the paired
`texture_width`/`texture_height`, `blend`, and inclusive
`submitted_ordinal_first`/`submitted_ordinal_last`. The ordinal is a fallback
for blind bisection, never a RenderDoc EID. `{"cmd":"draw-filter","clear":true}`
removes the predicate and restores unmodified replay.

### Information required at the seam

`RenderCmd` snapshots the resolved logical texture ID, dimensions, blend state,
and geometry ranges at recording time. The texture registry stores
`{void*, width, height}` behind each logical ID; the one-entry lookup cache
returns the same POD already needed to resolve the GPU texture. `GlobalBitmap`
registers its source dimensions, while dynamic textures retain dimensions from
`EnsureTexture`.

The replay seam uses only this compact metadata and an incrementing submitted
ordinal. It never exposes a raw `SDL_GPUTexture*` through the socket protocol.

### Cost model

Replay already iterates the command list before the command-buffer submit.
A filter at the replay seam adds one predicate evaluation per replayed geometry
command and no additional list traversal. Captures used in #621 contained about
500--2500 relevant draw events per frame; this is a useful upper operating
range, not a proof that RenderDoc EIDs equal `RenderCmd` entries.

The filter must stay allocation-free and avoid string parsing in replay. Parse
and validate the JSON request once; replay consumes an immutable compact
predicate.

### Legacy OpenGL parity

The historical OpenGL renderer at `a410f3de` has no equivalent single seam.
Two RHI final draw sites cover IR/RHI draws, but four BMD `glDrawArrays` sites
and eleven terrain immediate-mode `glBegin` sites bypass that path. A filter
only in the RHI would be intentionally incomplete.

SDL GPU filtering is therefore independently viable. Legacy parity needs a
separate decision:

- accept partial RHI-only coverage;
- add dedicated hooks for RHI, BMD, and immediate terrain; or
- defer legacy filtering until a common draw-record abstraction exists.

It must not claim ordinal or EID parity until a capture correlates each backend's
actual draw sequence.

## Baseline measurements

| Concern | Measured result | Source |
|---|---:|---|
| Release console branches | 26 | `src/source/Core/Utilities/Log/muConsoleDebug.cpp:89-247` |
| Debug-only console branches | 12 | `src/source/Core/Utilities/Log/muConsoleDebug.cpp:250-348` |
| Socket dispatch limit | 16 requests/connection/frame | `src/source/App/Control/ControlServer.h:28-32` |
| SDL GPU `RenderCmd` appends | 13; 12 outside `_EDITOR` | `src/source/Render/Renderer/MuRendererSDLGpu.cpp` |
| SDL GPU geometry producers | 6 | `MuRendererSDLGpu.cpp:2569-3358` |
| SDL GPU geometry replay seam | 1 (`ReplayDrawCommand`) | `MuRendererSDLGpu.cpp:780-816` |
| Legacy raw draw sites | 6 (`glDraw*`); plus 11 terrain `glBegin` sites | `a410f3de:src/source/Render/RHI/RHI_GL.cpp`, `Render/Models/ZzzBMD.cpp`, `Render/Terrain/ZzzLodTerrain.cpp` |
| #621 capture range | approximately 500--2500 relevant draw events/frame | capture investigation record |

## Live counter probe

A counter that does not move under a change we cause on purpose is not a probe,
so `stats` was accepted against the running client rather than against its unit
test. Lorencia, the current SDL GPU renderer, `testgmDw`:

```
frame advances            frame 490 -> 611 -> 733 -> 852 (median ~59.5 fps)
baseline                  requested=3300 submitted=2561 replayed=2579
draw-filter blend=false   requested=3439 submitted=1229 replayed=2588
draw-filter clear         requested=3587 submitted=2569 replayed=2587
```

Three things worth keeping from this:

- `submitted` more than halves under the filter and comes back when it is
  cleared, so the counter reports what the GPU was actually asked to draw.
- `replayed` does not move, because `s_dbgRenderCmdsReplayedThisFrame` counts
  commands entering the replay loop and the filter drops them one level below,
  at the top of `ReplayDrawCommand`. Filtering measures submission, not recording.
- The first read after entering the world reported `frame 389 -> 390` across a
  whole second. That is the map-load stall, not a frozen snapshot: a sweep has
  to let the scene settle before it samples, or it will compare a loading frame
  against a running one.

## Thirty-map counter sweep

The counters were then read on every map the server can place a character on and
the client can load: thirty maps, one client process each, the character moved
between them by SQL with the client closed. Two identities held exactly on all
thirty:

```
requested == merged + submitted              every map, no residue
geometry  == submitted + dropped + filtered  every map, no residue
```

The first one settles what the submitted-per-requested ratio measures. It ranges
from 0.139 on Kanturu Event to 0.769 on Lost Tower against a 0.52 median, and the
whole spread is merging: Kanturu Event merged 2615 of 3036 requested draws. The
ratio carries no defect signal, so nothing should be built on it.

The second identity had to be created. Before it, a draw that the replay loop
walks and then abandons at the bind guard could only be seen as
`replayed - submitted`, and that difference also counts the loop's SetViewport,
SetScissor and DebugLabel commands. It measured exactly 18 on all thirty maps --
indistinguishable from a permanent silent drop of 18 objects. Splitting geometry
commands from dropped and filtered ones resolved it: the 18 are state commands,
`dropped` is zero, and `geometry == submitted` on both a median map and the
extreme outlier.

A dropped draw is worth its own counter because nothing else reports one. The
texture lookup succeeds when the command is recorded, so `fallbackTextureDraws`
stays silent while the object is absent from the frame -- a visual defect with no
crash and no failing test, which is the class #547 warned would come back.

## Implementation boundaries

The console command and draw filter should each be a deep module with one small
control-socket interface. Their implementations may depend on the legacy parser
and renderer internals, but request parsing, validation, response formatting,
and per-frame renderer replay must not be copied into every command producer.

The hard problems are different:

- **Console routing:** isolate the console parser from the broader chat/UI
  wrapper and make malformed legacy arguments safe for the main loop.
- **Draw filtering:** preserve useful identity through deferred recording and
  merging. Matching an exact RenderDoc event is not possible from the current
  static data alone.

## Source map

- Control socket contract: `docs/control-socket.md`.
- Command parser: `src/source/Core/Utilities/Log/muConsoleDebug.cpp`.
- Chat command wrapper: `src/source/Engine/Object/ZzzInterface.cpp`.
- SDL GPU deferred replay: `src/source/Render/Renderer/MuRendererSDLGpu.cpp`.
- Deferred renderer invariants: `docs/GPU Skinning/immediate-renderer-architecture.md`.
- Renderer gotchas: `docs/GPU Skinning/gotchas-and-patterns.md`.
