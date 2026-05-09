# Inkternity — Phase 2 Design Doc

> **Audience:** the agent (and any human contributor) working inside the Inkternity repo.
>
> **Goal of this doc:** define what Phase 2 ships on top of the Phase 1 feature set, where each new piece slots into the existing Inkternity + InfiniPaint codebase, what is in scope, and what is explicitly out of scope.

## 1. Product summary

Phase 2 sharpens Inkternity into a comic-production tool that's meaningfully more useful than the sum of its Phase 1 pieces. Three additions:

1. **Pixel-to-vector** — convert an ink-brush raster region (libmypaint tiles) into vector paths so artists can apply the existing transform/edit/recolor tooling on top of work originally drawn with raster brushes.
2. **Per-waypoint transition controls** — speed multiplier (and optionally easing curve) so the reader-mode camera transition between waypoints can feel snappy, lingering, dramatic, etc., per panel.
3. **Sketch + production layers** — two semantically-distinct layer kinds with hard top-bar toggles, where only the production layer is visible in reader mode. Models the standard ink-over-pencils workflow.

Phase 2 ships under the same `.inkternity` extension; format bumps as needed for new on-disk fields.

## 2. Inheritance from Phase 1

Phase 1 left the following bits in shipped, working condition:

- libmypaint brush integration (`MyPaintBrushTool`, `MyPaintLayerCanvasComponent`, `LibMyPaintSkiaSurface`) with persistent tile data
- Waypoint data model (`Waypoint`, `WaypointGraph`, `Edge`) with NetObj sync, save/load, scale-up
- Tree-view side-panel editor (drag/select/connect/delete)
- Reader mode (toggle, chrome hide, arrow nav, branch-choice overlay with skin/back/dead-end handling)
- Waypoint skins (`ButtonSelectTool` capture, PNG storage)
- Ink toolbar icon

Plus the `BezierEasing` + `DrawingProgramLayerManager` + `DrawingProgramSelection` machinery inherited from InfiniPaint. Phase 2 builds on this rather than replacing it.

## 3. Workstream A — Pixel-to-vector

### Why

libmypaint brushes produce expressive, natural-media linework — but it's stuck inside a `MyPaintLayerCanvasComponent`'s tile grid. None of Inkternity's vector tooling (RectSelect transform, EditTool color edit, layer reorder of individual strokes, Lasso, copy/paste of objects) can act on a libmypaint stroke; the whole layer is one opaque blob from the rest of the program's point of view. Pixel-to-vector cuts a chunk of raster out of that blob and replaces it with vector canvas components the rest of Inkternity already knows how to manipulate.

The user's framing: ink in raster mode for the brush feel, then convert to vector for downstream tooling.

### Tool surface

A new `PixelToVectorTool` (`DrawingProgramToolType::PIXEL_TO_VECTOR`) — registered alongside existing tools, toolbar icon `data/icons/pixel-to-vector.svg`. UX:

1. User selects the tool.
2. User drags a rectangle (mirror `RectSelectTool` / `ButtonSelectTool` interaction) over the canvas region they want to convert.
3. On release: the tool composites every `MyPaintLayerCanvasComponent` tile **that lives on a PRODUCTION-kind layer** intersecting the rect into one bounded RGBA bitmap, runs the conversion, and inserts new vector canvas components representing the result *into the same PRODUCTION layer*. The original raster pixels in that rect are erased from the PRODUCTION layer (eraser dabs into the same MyPaint surfaces, mirroring `MyPaintLayerCanvasComponent::erase_along_segment`).

**Sketch-layer pixels are explicitly excluded** from both the source composite and the erasure. Sketch is a raster-only scratch surface by design (Workstream C); pixel-to-vector ignores anything painted there. If the artist drags the tool over a region with no PRODUCTION raster pixels, it's a no-op.

Rationale for "erase the source": leaving both in place would double-render the stroke and confuse the artist about which copy is authoritative. Replacement is the correct mental model.

### Conversion algorithm — Phase 2A (alpha-channel single-color)

Scope decision: Phase 2 ships **alpha-channel-only** tracing, treating the masked region as a single-color shape. Most ink/marker work is single-color anyway; the multi-color case is dramatically more complex (color quantization + per-band tracing + overlap resolution) and slots into Phase 3.

The pipeline:

1. **Composite** — render every intersecting MyPaint tile into a flat RGBA bitmap aligned to the selection rect. Produces width × height × RGBA.
2. **Mask** — threshold the alpha channel at α ≥ 64 (configurable later). Yields a binary image.
3. **Color sample** — compute the mean RGB of pixels above threshold, weighted by alpha. This becomes the fill color of the resulting vector shape.
4. **Marching squares** — extract iso-contours along the binary boundary. Outputs polygonal loops (outer boundary + any holes).
5. **Douglas–Peucker simplification** — collapse near-collinear runs. Tolerance scales with the brush size used in the source region (a future refinement; initial cut uses a fixed tolerance of ~0.75 px).
6. **Schneider bezier fit** (Schneider 1990, the algorithm Inkscape's "Trace Bitmap" uses post-potrace) — fit cubic Béziers to the simplified polygons within an error tolerance, producing smooth `SkPath` segments.
7. **Emit** — wrap each resulting `SkPath` + fill color in a brush canvas component (the existing `BrushTool`-flavored container that holds vector strokes), insert into the active layer, register undo.

The whole pipeline is single-threaded and synchronous; for a typical selection (a few hundred pixels per side, a handful of contour loops) it should complete in tens of milliseconds.

### Library / dependency choice

**No new dependencies.** Surveyed options:

- **Potrace** — the gold standard for binary-image tracing, but GPLv2; incompatible with Inkternity's MIT license. Rule out.
- **autotrace** — also GPL. Rule out.
- **VTracer** (Apache-2.0) — handles multi-color but Rust-only; would force a Rust toolchain into the Conan/CMake build pipeline. Heavyweight for what Phase 2A needs.
- **OpenCV** — `cv::findContours` + `cv::approxPolyDP` give us steps 4–5 cheaply, Apache-2.0. But OpenCV is a >100 MB dependency for what is realistically <500 lines of code.
- **Custom in-tree implementation** — marching squares (~80 lines), Douglas-Peucker (~40 lines), Schneider bezier fit (~300 lines, well-known algorithm with reference implementations in the public domain). All fits in a single new `src/RasterToVector/` directory.

We go with **custom**. Phase 2 is the right time to take the dep-cost cut: tracing is core to the Inkternity workflow, not a passing feature, and the algorithms are stable and small.

### Risks

- **Raster-to-vector visual fidelity.** Cubic-bezier-fit-of-marching-squares is not pixel-perfect. Sharp inner corners get slightly rounded; very thin strokes can become disconnected dots. Mitigation: stroke-width-aware tolerance + a "preview before commit" overlay so the artist sees the trace and can cancel before erasure.
- **Selection that crosses zero MyPaint pixels.** Tool is a no-op (do nothing, keep raster intact, no error popup).
- **Selection that crosses a layer with mixed raster + vector components.** Vector components in the rect are left alone — only raster pixels get consumed. This is the obvious-correct behavior; just make sure we don't accidentally delete the vector components.
- **Erasure under undo.** The erase + insert must be one undo-able action. Use the existing `add_undo_place_components` + a new undo entry for the erased tile data (probably easiest as a tile-snapshot before/after pair).

### Subtasks

| | Deliverable |
|---|---|
| A1 | `RasterToVector` library: marching squares + Douglas-Peucker + Schneider fit, unit tests on synthetic shapes |
| A2 | `PixelToVectorTool` skeleton: registered in toolbar (desktop + phone), rect-drag UI, no conversion yet |
| A3 | Tile composite extraction: walk MyPaintLayer components in selection rect, render to RGBA bitmap |
| A4 | End-to-end conversion: composite → mask → trace → emit vector canvas components |
| A5 | Source erasure + single-undo entry covering both insertion and erasure |
| A6 | Preview overlay before commit (artist confirms before raster destruction) |

## 4. Workstream B — Waypoint transition speed (and easing)

### Why

Reader-mode camera transitions today use a single global speed (`MainProgram::conf.jumpTransitionTime`) and a single global easing (`MainProgram::conf.jumpTransitionEasing`). For comics this is too uniform: an action panel cut wants a fast, crisp transition; a quiet establishing shot wants a slow, deliberate one. Per-waypoint control lets the artist pace the read.

### What's already there

The DrawCamera animation engine already supports configurable time + easing per call:

```cpp
// DrawCamera.cpp:67
float smoothTime = smooth_two_way_animation_time_get_lerp(
    smoothMove.moveTime,
    w.main.deltaTime,
    true,
    w.main.conf.jumpTransitionTime);   // <-- duration source
BezierEasing zoomAnim{w.main.conf.jumpTransitionEasing};  // <-- easing source
```

`BezierEasing` is a CSS-style cubic-bezier (`x1, y1, x2, y2` control points, stored as `Eigen::Vector4f`). It already has a `BezierEasing::linear` static. Easing presets cost essentially nothing.

### Approach

1. Add to `Waypoint`:
   - `float transitionSpeedMultiplier = 1.0f;` (range 0.1–2.0, displayed as a slider; the *target* duration becomes `globalDuration / multiplier` so 2.0 means twice as fast)
   - `Eigen::Vector4f easing;` defaulting to the CSS "ease" curve `(0.25, 0.1, 0.25, 1.0)`. Also expose presets via dropdown:
     - `Linear` (0,0,1,1)
     - `Ease` (0.25,0.1,0.25,1) — default
     - `Ease In` (0.42,0,1,1)
     - `Ease Out` (0,0,0.58,1)
     - `Ease In Out` (0.42,0,0.58,1)
2. Extend `DrawCamera::smooth_move_to` to accept optional `durationOverride` + `easingOverride` parameters (defaulting to `nullopt`, in which case the global config values still apply).
3. `ReaderMode::snap_camera_to_current` reads the destination waypoint's overrides and passes them through.
4. `WaypointTool` settings panel adds the slider (below the existing label field) and the easing dropdown.
5. Save/load: append to `Waypoint::save_file` / `load_skin_from_archive` (well, refactor the load to pull these too); format bump 0.7 → 0.8. Old files default both fields.

### Risks

- **Easing is genuinely optional.** The user flagged this as "if it's easy" — `BezierEasing` makes it trivial, so include it. If the dropdown clutters the waypoint settings panel, hide it behind a "More…" disclosure.
- **Multiplier semantics.** `0.1×` means *slower*, `2.0×` means *faster*. Slider label should be explicit (e.g. "Transition speed: 1.5× — faster"). Don't quietly invert what the value means.

### Subtasks

| | Deliverable |
|---|---|
| B1 | `Waypoint` data model: `transitionSpeedMultiplier` + `easing` fields, save/load, format bump |
| B2 | `DrawCamera::smooth_move_to` overload accepting per-call duration + easing |
| B3 | `ReaderMode` plumbing: pass per-waypoint overrides through |
| B4 | `WaypointTool` settings UI: slider + easing dropdown |

## 5. Workstream C — Sketch + production layers

### Why

The standard pipeline for any inked comic is rough → ink → color → flatten. Inkternity's flat single-layer-stack inherits from a general-purpose canvas tool, but for comic production it's worth modelling the rough/finished split as a first-class concept the program understands. The program understanding it means reader mode can reliably hide the rough sketch and the artist doesn't have to remember to manually toggle layer visibility before publishing.

### What's already there

- `DrawingProgramLayerManager` with `editingLayer` (NetObjWeakPtr to currently-active layer)
- `DrawingProgramLayerListItem` items inside a `DrawingProgramLayerFolder` tree
- Reader mode visibility filter pattern: `if (world.readerMode.is_active()) return;` is used in 8+ places already
- New worlds auto-create one layer ("First Layer") in `DrawingProgramLayerManager`

### Approach

Add a `LayerKind` enum to `DrawingProgramLayerListItem` (NOT to the layer's component list — the kind belongs to the layer's *role*, not its data):

```cpp
enum class LayerKind : uint8_t {
    DEFAULT = 0,     // generic layer; visible in both modes
    SKETCH = 1,      // hidden in reader mode; "rough" semantic
    PRODUCTION = 2   // visible in both modes; "finished" semantic
};
```

The `DEFAULT` value preserves backward compat: existing files (and InfiniPaint-format files we still load) get `DEFAULT` for every layer, which behaves exactly as today.

**On new world creation:** auto-create two layers instead of one — `Sketch` (kind=SKETCH) and `Production` (kind=PRODUCTION). `editingLayer` defaults to PRODUCTION.

**Render filter:** in `DrawingProgramCache::recursive_draw_layer_item_to_canvas` (or its callers), skip layers whose kind is SKETCH while reader mode is active. Other kinds render normally.

**Top-bar UI:** two pill buttons in the top toolbar (visible only outside reader mode), labeled "Sketch" and "Production". Clicking either flips `editingLayer` to that layer. The currently-active one shows as selected. This is the artist's only interaction with these layers — the layer-manager side panel still works for other layers, but the two named layers can't be deleted/renamed.

**Constraints to enforce:**
- Sketch + Production layers are protected from the layer manager's delete/rename UI (tooltip explains why).
- A document always has exactly one of each (lazy-create on load if missing — handles legacy files gracefully).
- They sit at fixed positions in the layer order: PRODUCTION on top of SKETCH (so ink appears over the rough). Other user-added layers can be inserted between them or above/below.
- **Sketch is raster-only.** Vector canvas components (BrushTool strokes, shapes, textboxes) cannot be added to the sketch layer; the layer accepts only `MyPaintLayerCanvasComponent`s. This is enforced at the editing-layer-target check: when `editingLayer` is the sketch layer, the vector tools are visually disabled (or auto-switch the active layer to PRODUCTION on first vector-tool action — pick one and document it). Pixel-to-vector mirrors this: it doesn't read from or write to the sketch layer (see Workstream A).

### Risks

- **Existing per-layer manipulation features (visibility toggle, blend mode, opacity) shouldn't be disabled for the named layers** — the artist may want to dim the sketch to 30% opacity while inking. Just delete + rename get blocked.
- **The top-bar toggle is "set active layer," not "set visibility."** Calling these "toggles" is overloaded — make sure UI copy makes clear that clicking switches your edit target, not the layer's display.
- **Multi-document case.** Each World has its own LayerManager so the named layers are per-document. Top-bar UI binds to the active world's named layers. No cross-document state.
- **NetObj sync.** LayerKind is a small POD field that reads/writes alongside existing layer constructor data; no protocol-level surprises expected.

### Subtasks

| | Deliverable |
|---|---|
| C1 | `LayerKind` enum on `DrawingProgramLayerListItem`, with NetObj save/load + format bump |
| C2 | New-world auto-creation of Sketch + Production layers; legacy-file lazy-create on load |
| C3 | Render filter: hide SKETCH-kind layers in reader mode |
| C4 | Top-bar pill toggles for active edit target |
| C5 | Layer-manager protection: disable delete/rename for named layers |

## 6. File format

Phase 2 bumps the format header from `INFPNT000008` (v0.7) to `INFPNT000009` (v0.8). New on-disk fields:

- Per-waypoint: `transitionSpeedMultiplier` (float), `easing` (Eigen::Vector4f) — read at version ≥ 0.8, default values otherwise.
- Per-layer-list-item: `layerKind` (uint8) — read at version ≥ 0.8, defaults to `DEFAULT` otherwise. Lazy-creation logic on load handles older files that don't have explicit Sketch/Production layers.

The pixel-to-vector workstream produces ordinary brush canvas components; no new on-disk type.

## 7. Out of scope for Phase 2

- **Multi-color raster tracing.** Phase 2A traces alpha→single-color only. Full multi-color quantize-and-trace lands in a later phase if usage demand justifies it.
- **Per-stroke trace.** Pixel-to-vector operates on rectangular regions only; no flood-fill-style "trace this connected blob" mode.
- **User-editable easing curves.** Phase 2 ships a fixed set of presets. A custom-curve editor (CSS cubic-bezier visualizer) is a Phase 3 concern.
- **More than two named layers.** Inks/pencils only; no separate "color layer" or "tones layer" kind. Artists can still create those manually as DEFAULT-kind layers.
- **Reader-mode preview of "what will the production view look like?"** while editing. Toggle reader mode on briefly is the workaround.
- **GPU-accelerated tracing.** Custom CPU implementation is fast enough for the expected selection sizes. Optimization slot for later.

## 8. Milestones

| # | Status | Milestone | Deliverable |
|---|---|---|---|
| M1 | | RasterToVector library | `src/RasterToVector/{MarchingSquares,DouglasPeucker,SchneiderFit}.{cpp,hpp}` + unit tests against synthetic ink shapes |
| M2 | | PixelToVectorTool end-to-end | Toolbar entry, rect-drag selection, composite extraction, conversion, vector emission, source erasure, single-undo |
| M3 | | Per-waypoint speed slider | Data model field, save/load, `DrawCamera` override path, `WaypointTool` settings UI |
| M4 | | Per-waypoint easing dropdown | Optional addition layered on top of M3 — preset list, dropdown UI, applied in transitions |
| M5 | | Sketch + Production layer kinds | `LayerKind` enum, auto-creation on new world, lazy-create on legacy load, render filter, top-bar toggles, manager-UI protection |
| M6 | | Format bump 0.8 + migration tests | `INFPNT000009` header, version-gated load paths for new fields, manual round-trip verification of legacy and current files |

M1 + M2 are the largest workstream (Pixel-to-vector). M3 + M4 share infra and are small. M5 is medium. M6 is bookkeeping. M1-M2 and M3-M5 can run in parallel.

## 9. Open product questions

Not blocking — flag and decide as Phase 2 progresses.

- **Trace-preview UX.** Show as a translucent overlay before erasing the source? Or commit immediately and let the artist undo? Probably the former.
- **Sketch-layer default opacity.** Should auto-created sketch layer come pre-set to a reduced opacity (~50%) so it visibly behaves "rough" out of the box, or leave at 100%?
- **Top-bar toggle position.** Existing top toolbar is densely populated on phone layout; does the Sketch/Production pair need a dedicated row, or fit alongside the eye-toggle?
- **Pixel-to-vector for non-MyPaint raster.** If a future raster source exists (e.g. imported PNG layer), should the same tool work on it, or stay MyPaint-tile-specific?
- **Should Pixel-to-vector run on a worker thread** so the UI doesn't stall on large selections? Profile after M2 lands; defer until measured slowness is real.

## 10. Decisions log

| Date | Decision | Reasoning |
|---|---|---|
| 2026-05-08 | Pixel-to-vector ships alpha-channel-only in Phase 2 | Multi-color tracing is 5× more complex and the ink/marker use case is dominant. Multi-color is a later phase if demand emerges. |
| 2026-05-08 | Custom in-tree raster-to-vector library, not Potrace/VTracer/OpenCV | Potrace/autotrace are GPL (incompatible). VTracer requires Rust toolchain. OpenCV is a giant dep for ~500 lines of work. Custom keeps build clean and total LOC manageable. |
| 2026-05-08 | Pixel-to-vector erases source raster after conversion | Keeping both copies confuses authoritative state. Replacement is the correct mental model; preview-before-commit handles the "what if I don't like it" case. |
| 2026-05-08 | Waypoint transition controls reuse existing `BezierEasing` infra | The animation engine already supports per-call duration + easing curves; adding per-waypoint overrides is plumbing, not new infrastructure. |
| 2026-05-08 | Sketch/Production layers are a `LayerKind` enum, not separate types | Keeps the existing layer hierarchy intact; the kind is metadata on a normal layer. `DEFAULT` value preserves backward compat for InfiniPaint-format files. |
| 2026-05-08 | Auto-create both named layers on new world; lazy-create on legacy load | Avoids forcing the user through a setup step; legacy files just get the layers added on first open. |
| 2026-05-08 | Sketch layer is raster-only; only Production layer is eligible for pixel-to-vector | Models the standard rough-then-ink pipeline cleanly. Sketch becomes a strict scratch surface — no vector pollution, no ambiguity about which layer the converted vectors land in (always Production). |
