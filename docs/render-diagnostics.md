# Render Diagnostics

> **Status:** the `console` control is implemented. The draw filter remains a design baseline.

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

Twelve additional branches are Debug-only: `$open`, `$close`, `$clear`,
`$type_test`, `$texture_info`, `$color_test`, `$mapatt on/off`, `$path on/off`,
and `$bb on/off`. They remain unavailable in Release; the new socket command
must report that state instead of inventing a second implementation.

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

### Proposed interface

The external control is a frame-scoped predicate with all clauses optional:

```json
{
  "cmd":"draw-filter",
  "disable": {
    "submitted_ordinal":{"first":120,"last":240},
    "texture_id":12778,
    "texture_size":{"width":16,"height":16},
    "blend":{"enabled":false,"mode":"alpha"}
  }
}
```

A draw matches only when every supplied clause matches. The default is disabled;
clearing the filter restores unmodified replay. The request names
`submitted_ordinal` specifically so callers do not mistake it for a RenderDoc
EID. Range bounds are inclusive.

The JSON shape is a proposal, not an implemented control-socket contract. It
must retain one explicit choice before implementation: whether several filters
combine as OR rules or whether one request creates one AND predicate. The shape
above describes the latter because it makes a live bisection unambiguous.

### Information required at the seam

`RenderCmd` already snapshots type, pipeline, texture pointer, sampler, vertex
and index ranges, viewport/scissor, and some blend/depth/cull state. It is
sufficient for:

- a submitted ordinal generated in replay;
- type, pipeline, texture-pointer, and geometry-range predicates;
- blend predicates for ordinary text, 2D-quad, triangle, and 3D-quad commands.

It is insufficient for the full proposed interface:

- no logical texture ID is stored in `RenderCmd`;
- no texture width or height is stored in `RenderCmd`;
- skinned-triangle and quad-strip producers do not snapshot the explicit
  blend/depth/cull fields, even though their selected pipeline embodies state.

Therefore a complete filter needs command metadata captured at recording time:
logical texture ID, texture dimensions when known, and consistent state
snapshots for every geometry producer. A raw `SDL_GPUTexture*` must not become
the socket protocol.

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
