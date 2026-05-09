# Inkternity — Phase 2 Design Doc

> **Audience:** the agent (and any human contributor) working inside the Inkternity repo.
>
> **Goal of this doc:** define what Phase 2 ships on top of the Phase 1 feature set, where each new piece slots into the existing Inkternity + InfiniPaint codebase, what is in scope, and what is explicitly out of scope.

## 1. Product summary

Phase 2 sharpens Inkternity into a comic-production tool that's meaningfully more useful than the sum of its Phase 1 pieces. Three additions:

1. **Stroke vectorization** — convert recorded libmypaint strokes inside an ink-layer region into vector paths so artists can apply the existing transform/edit/recolor tooling on top of work originally drawn with raster brushes. Per-stroke recording at draw time means the converter knows each stroke's exact color and source path; no marching-squares fidelity loss, no multi-color tracing problem.
2. **Per-waypoint transition controls** — speed multiplier (and optionally easing curve) so the reader-mode camera transition between waypoints can feel snappy, lingering, dramatic, etc., per panel.
3. **Three-layer pipeline (Sketch → Color → Ink)** — three semantically-distinct layer kinds modelling the standard rough-color-ink production order. Sketch is hidden in reader mode (rough scratch space, raster-only); Color and Ink both render in reader mode. The active edit target is selected via a top-bar layer dropdown.

Phase 2 ships under the same `.inkternity` extension; format bumps as needed for new on-disk fields (stroke metadata + per-layer kind + per-waypoint transition fields).

## 2. Inheritance from Phase 1

Phase 1 left the following bits in shipped, working condition:

- libmypaint brush integration (`MyPaintBrushTool`, `MyPaintLayerCanvasComponent`, `LibMyPaintSkiaSurface`) with persistent tile data
- Waypoint data model (`Waypoint`, `WaypointGraph`, `Edge`) with NetObj sync, save/load, scale-up
- Tree-view side-panel editor (drag/select/connect/delete)
- Reader mode (toggle, chrome hide, arrow nav, branch-choice overlay with skin/back/dead-end handling)
- Waypoint skins (`ButtonSelectTool` capture, PNG storage)
- Ink toolbar icon

Plus the `BezierEasing` + `DrawingProgramLayerManager` + `DrawingProgramSelection` machinery inherited from InfiniPaint. Phase 2 builds on this rather than replacing it.

## 3. Workstream A — Stroke vectorization

### Why

libmypaint brushes produce expressive, natural-media linework — but it's stuck inside a `MyPaintLayerCanvasComponent`'s tile grid. None of Inkternity's vector tooling (RectSelect transform, EditTool color edit, layer reorder of individual strokes, Lasso, copy/paste of objects) can act on a libmypaint stroke; the whole layer is one opaque blob from the rest of the program's point of view. Stroke vectorization breaks out individual strokes and replaces them with vector canvas components the rest of Inkternity already knows how to manipulate.

The user's framing: ink in raster mode for the brush feel, then convert to vector for downstream tooling.

### The architectural choice: per-stroke recording, not pixel tracing

The naïve approach is "pixel-trace the rendered tiles" — composite the tiles into a bitmap, run marching squares, fit Béziers, emit vectors. This works but loses fidelity at every step (corner rounding, thin-stroke breakup, color averaging) and breaks down on multi-color regions because the artist's freedom to pick any ink color makes "single-color tracing" insufficient.

Phase 2 takes the better path: **record every libmypaint stroke at draw time** as an ordered list of dabs (position + radius + color + pressure-derived alpha) alongside the tiles those dabs already paint into. The tile data stays the same — that's what gets rendered for the brush feel — but a parallel stroke log preserves the *intent* of each stroke. Vectorization then walks the stroke log inside the selection and emits one vector path per recorded stroke, with that stroke's exact color and a pressure-aware variable-width spine.

Concretely, this gives us:

- **Perfect color preservation** per stroke; multi-color ink layers convert correctly because each stroke is by definition single-color.
- **Perfect ordering** — strokes emit in the order they were drawn, so over-strokes layer correctly.
- **Pressure-derived width variation** preserved as variable-width spines (cubic-Bezier polylines with per-vertex width).
- **No marching-squares detail loss** — the source path *is* the vector, modulo a smoothing pass.

The cost is real but contained: `MyPaintLayerCanvasComponent` grows a stroke log alongside its tiles; the eraser must operate on both (raster dabs *and* trim/split recorded stroke segments); the file format adds a per-layer stroke list; pre-rebrand `.infpnt` files and `.inkternity` files written before this change have no stroke log at all and so cannot be vectorized (graceful degradation: the convert tool is a no-op on those layers, with a tooltip explaining why).

### Tool surface

A new `StrokeVectorizeTool` (`DrawingProgramToolType::STROKE_VECTORIZE`) — registered alongside existing tools, toolbar icon `data/icons/pixel-to-vector.svg`. UX:

1. User selects the tool.
2. User drags a rectangle (mirror `RectSelectTool` / `ButtonSelectTool` interaction) over the canvas region they want to convert.
3. On release: the tool collects every recorded libmypaint stroke on **INK-kind layers** whose dab path intersects the selection rect, emits each one as a vector canvas component into the same INK layer, and erases the source stroke (both the recorded log entry and the rasterized dabs from the tiles, via existing eraser-dab geometry along the stroke spine).
4. A translucent **preview overlay** of the converted vectors renders above the source raster before commit; user can confirm or cancel.

**Strokes on SKETCH and COLOR layers are ignored.** Sketch is raster-only scratch by design (Workstream C); color is freely raster-or-vector but its pre-ink state is intentionally left alone. If the selection contains zero recorded strokes on an ink layer, the tool is a no-op (no error popup).

Strokes that *partially* intersect the selection are converted in full, not split — the rect picks "this stroke or not", not "this fragment of a stroke." Treats a stroke as the smallest atomic unit. (Splitting strokes geometrically is doable but not Phase 2 scope.)

### Conversion pipeline (per-stroke)

For each recorded stroke selected for conversion:

1. **Walk the dab list.** Project each dab's center to canvas-space coords (the recorded coords are already in MyPaint surface space, which is canvas space).
2. **Smooth the polyline.** Three-tap centered moving average to remove micro-jitter from high-rate sampling. Preserves stroke shape.
3. **Fit cubic Béziers** along the smoothed polyline using Schneider's least-squares fit (1990) — the same algorithm Inkscape uses for path simplification. Tolerance scales with the dab radius so small details on fine pens stay sharp while broad-marker strokes simplify aggressively.
4. **Emit a variable-width brush stroke**: the existing `BrushTool` vector format already supports per-vertex width (driven by tablet pressure on a vector brush). We populate width from the dab radius × pressure-derived alpha at each fitted control point.
5. **Set the stroke color** from the recorded dab color (constant per stroke).
6. **Wrap in a brush canvas component**, push to the layer's component list, register undo.

The whole pipeline is single-threaded and synchronous; per-stroke cost is O(dabs), and a typical ink stroke has hundreds of dabs at most. A selection containing dozens of strokes converts in single-digit milliseconds.

### Library / dependency choice

**No new dependencies.** All work is custom in-tree:

- Stroke recording (~150 lines): hook `MyPaintBrushTool::input_pen_motion` and the begin/end_atomic boundary in `LibMyPaintSkiaSurface` to capture dabs.
- Schneider Bezier fit (~300 lines): well-known algorithm, public-domain reference implementations exist.
- Smoothing + projection: trivial.

Total surface is comparable to the abandoned pixel-tracing approach, with strictly better output and no library decisions to relitigate later.

### Risks

- **Eraser handling.** Phase 1's eraser (`erase_along_segment`) only punches alpha into tiles. Now it also has to mark recorded strokes (or stroke fragments) as erased. Phase 2 ships the *simplest* correct behavior: if the eraser touches a recorded stroke, that stroke is marked fully consumed (removed from the log entirely) regardless of how much of it was actually erased. Vectorization output then doesn't include the partially-erased stroke. The visual raster still shows the partial erasure correctly. This loses some convertibility for "I just touched the edge with the eraser" cases — accept it, defer geometric stroke-splitting to a later phase.
- **Loaded-from-old-file degradation.** Files written before Phase 2 have no stroke log. The vectorize tool detects this and is a no-op on those layers (tooltip: "No recorded stroke data — strokes drawn before Phase 2 cannot be vectorized."). New strokes drawn into those layers post-load *do* record properly, so the layer becomes partially convertible.
- **Stroke-log file size.** Each dab is ~24–32 bytes (position, radius, pressure, color). A heavily-inked panel with hundreds of strokes × hundreds of dabs each could add a megabyte or two per layer. Acceptable. zstd-compress the log on save (cereal already supports binary_data through zstd in the existing tile path).
- **Selection across multiple INK layers.** Convert each stroke into the layer it was originally drawn on, not the active layer. (User probably expects "convert in place," not "consolidate into active layer.") Document this clearly in the tool's tooltip.

### Subtasks

| | Deliverable |
|---|---|
| A1 | Stroke-recording infra in `LibMyPaintSkiaSurface` + `MyPaintBrushTool`: capture dabs at draw time into a per-layer log; save/load log alongside tiles |
| A2 | Schneider bezier-fit module: `src/StrokeVectorize/SchneiderFit.{cpp,hpp}` + unit tests against synthetic dab chains |
| A3 | `StrokeVectorizeTool` skeleton: registered in toolbar (desktop + phone), rect-drag UI, no conversion yet |
| A4 | End-to-end conversion: walk strokes inside selection, fit, emit vector canvas components into source ink layer |
| A5 | Source removal: erase recorded strokes from the log and the corresponding raster dabs from tiles, all inside one undo entry |
| A6 | Eraser interaction: tile-erase still works; recorded strokes touched by eraser get fully removed from the log |
| A7 | Preview overlay before commit (translucent vectors over source raster; commit / cancel buttons) |
| A8 | Format integration: stroke log added to `MyPaintLayerCanvasComponent::save_file` / `load_file`; gated behind format version 0.8 |

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
    DEFAULT = 0,   // generic layer; visible in both modes (legacy / user-added)
    SKETCH  = 1,   // hidden in reader mode; raster-only rough
    COLOR   = 2,   // visible in both modes; flat color fills
    INK     = 3    // visible in both modes; line work, vectorization target
};
```

The `DEFAULT` value preserves backward compat: existing files (and InfiniPaint-format files we still load) get `DEFAULT` for every existing layer, which behaves exactly as today.

**Stack order, bottom to top:** SKETCH → COLOR → INK. This matches the production pipeline: rough at the bottom, color over the rough, ink lines on top to define the visible edges (and to mask any color bleed underneath, which is the trick that lets COLOR stay raster-only — the eye reads ink boundaries as the shapes' boundaries).

**On new world creation:** auto-create three layers instead of one — `Sketch` (kind=SKETCH, bottom), `Color` (kind=COLOR, middle), `Ink` (kind=INK, top). `editingLayer` defaults to INK.

**Render filter:** in `DrawingProgramCache::recursive_draw_layer_item_to_canvas` (or its callers), skip layers whose kind is SKETCH while reader mode is active. COLOR and INK render normally; DEFAULT renders normally (matches Phase 1 behavior).

**Top-bar UI:** a single layer dropdown (visible only outside reader mode) with three entries — Sketch / Color / Ink — showing the currently-active edit target. Clicking the dropdown opens the picker; selecting an entry flips `editingLayer` to that layer. Cleaner top-bar real-estate than three pill toggles, and reads obviously as a layer selector. The layer-manager side panel still works for other (DEFAULT-kind) layers and for visibility / opacity / blend-mode adjustments on the three named layers.

**Per-layer tool restrictions:**
- **SKETCH is raster-only.** Vector canvas components (BrushTool strokes, shapes, textboxes) cannot be added; the layer accepts only `MyPaintLayerCanvasComponent`s. When `editingLayer` is sketch and the user picks a vector tool, the tool is visually disabled (greyed out with tooltip). Stroke vectorization (Workstream A) doesn't read from or write to sketch.
- **COLOR is permissive.** Accepts both raster (libmypaint flat-fill marker, etc.) and vector (rect/ellipse fill, lasso, gradient) — the artist picks the right tool for the job. Stroke vectorization doesn't operate on color either, since color is intentionally left raster.
- **INK is permissive but is the only vectorization target.** Recorded libmypaint strokes drawn here are eligible for stroke vectorization; vector strokes drawn here (with the regular `BrushTool`) are already vectors and pass through unchanged.

**Constraints on the three named layers:**
- Cannot be deleted or renamed via the layer manager (tooltip explains why).
- Document always has exactly one of each (lazy-create on load if missing — handles legacy InfiniPaint files and pre-Phase-2 Inkternity files).
- Stack positions are loosely fixed: SKETCH at bottom, INK at top. Other user-added DEFAULT-kind layers can be inserted between them.
- Visibility / opacity / blend-mode toggles in the layer manager are NOT disabled — the artist may want a 30%-opacity sketch while inking, or a multiply-blend ink layer.

### Risks

- **The dropdown is "set active layer," not "set visibility."** UI copy must make clear that picking a layer switches the edit target, not what's displayed. Existing layer-manager visibility toggles continue to control display.
- **DEFAULT-kind layers from legacy files.** When an InfiniPaint-format file (or pre-Phase-2 Inkternity file) loads, all its existing layers come in as DEFAULT. We then lazy-create the three named layers on top. The user sees their old layers AND the three new empty ones. Acceptable; document in the open-questions if a "promote this layer to INK" flow becomes desirable.
- **Multi-document case.** Each World has its own LayerManager, so the three named layers are per-document. Top-bar dropdown binds to the active world's named layers. No cross-document state.
- **NetObj sync.** LayerKind is a small POD field that reads/writes alongside existing layer constructor data; no protocol-level surprises expected.

### Subtasks

| | Deliverable |
|---|---|
| C1 | `LayerKind` enum (4 values) on `DrawingProgramLayerListItem`, with NetObj save/load + format bump |
| C2 | New-world auto-creation of Sketch + Color + Ink layers in the right stack order; legacy-file lazy-create on load |
| C3 | Render filter: hide SKETCH-kind layers in reader mode |
| C4 | Top-bar layer dropdown for active edit target (Sketch / Color / Ink) |
| C5 | Layer-manager protection: disable delete/rename for the three named layers |
| C6 | Tool-restriction enforcement: vector tools greyed out when editingLayer is sketch |

## 6. File format

Phase 2 bumps the format header from `INFPNT000008` (v0.7) to `INFPNT000009` (v0.8). New on-disk fields:

- Per-waypoint: `transitionSpeedMultiplier` (float), `easing` (Eigen::Vector4f) — read at version ≥ 0.8, default values otherwise.
- Per-layer-list-item: `layerKind` (uint8) — read at version ≥ 0.8, defaults to `DEFAULT` otherwise. Lazy-creation logic on load handles older files that don't have explicit Sketch/Color/Ink layers.
- Per `MyPaintLayerCanvasComponent`: serialized stroke log (count + per-stroke (color, dab list with position/radius/pressure)) — read at version ≥ 0.8. Pre-0.8 components have no log; the convert tool detects this and is a no-op on them. zstd-compressed on disk via the existing binary_data path.

The stroke vectorization workstream produces ordinary brush canvas components; no new on-disk type beyond the stroke log.

## 7. Out of scope for Phase 2

- **Geometric stroke splitting under the eraser.** Phase 2 treats an eraser touch as removing the whole touched stroke from the recorded log (the rasterized partial erasure still shows correctly). Splitting strokes geometrically so a partially-erased stroke remains partially convertible is deferred.
- **Multi-color raster tracing.** Phase 2 doesn't ship a pixel tracer at all — stroke recording handles all the color cases the workflow actually produces. Pre-Phase-2 raster (legacy file load) cannot be vectorized; live with that.
- **User-editable easing curves.** Phase 2 ships a fixed set of presets. A custom-curve editor (CSS cubic-bezier visualizer) is a Phase 3 concern.
- **More than three named layers.** No separate "tones," "fx," or "lettering" kinds. Artists can still create extras manually as DEFAULT-kind layers; the dropdown only manages the three named ones.
- **"Promote DEFAULT layer to INK" flow.** Legacy files get the three named layers added on top of their existing layers. Re-classifying a legacy layer as INK so its libmypaint strokes (which weren't recorded pre-Phase-2 anyway) become eligible isn't worth the complexity.
- **Reader-mode preview of "what will the production view look like?"** while editing. Toggle reader mode on briefly is the workaround.
- **GPU-accelerated stroke fitting.** CPU implementation is fast enough at expected stroke counts.

## 8. Milestones

| # | Status | Milestone | Deliverable |
|---|---|---|---|
| M1 | | Stroke recording infra | Hook `MyPaintBrushTool` + `LibMyPaintSkiaSurface` to capture per-stroke dab logs; stash on the `MyPaintLayerCanvasComponent` alongside tiles |
| M2 | | Schneider bezier-fit module | `src/StrokeVectorize/SchneiderFit.{cpp,hpp}` + unit tests against synthetic dab chains |
| M3 | | StrokeVectorizeTool end-to-end | Toolbar entry, rect-drag selection, walk-strokes-in-rect, fit, emit vector components into source ink layer, source removal, single-undo, preview overlay before commit |
| M4 | | Per-waypoint speed slider | Data model field, save/load, `DrawCamera` override path, `WaypointTool` settings UI |
| M5 | | Per-waypoint easing dropdown | Layered on M4 — preset list, dropdown UI, applied in transitions |
| M6 | | Three-layer kinds (Sketch / Color / Ink) | `LayerKind` enum, auto-creation on new world, lazy-create on legacy load, render filter, top-bar dropdown, manager-UI protection, vector-tool greying on sketch |
| M7 | | Format bump 0.8 + migration tests | `INFPNT000009` header, version-gated load paths for stroke logs / layer kinds / waypoint transitions, manual round-trip verification of legacy and current files |

M1–M3 are the largest workstream (stroke vectorization) and depend in order. M4–M5 share infra and are small. M6 is medium. M7 is bookkeeping that touches all three workstreams. M1-M3 and M4-M6 can run in parallel; M7 closes them out.

## 9. Open product questions

Not blocking — flag and decide as Phase 2 progresses.

- **Sketch-layer default opacity.** Should auto-created sketch layer come pre-set to a reduced opacity (~50%) so it visibly behaves "rough" out of the box, or leave at 100%?
- **Layer dropdown placement on phone.** The phone top toolbar is densely populated. Does the dropdown live alongside the eye-toggle, or get its own slot?
- **Stroke log retention through transformations.** If the artist rect-selects a libmypaint-painted region and translates/rotates it via the existing transform tools, the recorded dab positions become stale relative to the moved tiles. Phase 2 ships "transform invalidates the log entries for moved strokes" (they remain in the layer as raster, just no longer convertible) — flag for revisit if this hurts the workflow.
- **Stroke vectorization as a worker thread.** CPU work per stroke is small but a 200-stroke selection could stutter the UI for a frame or two. Profile after M3 lands; defer until measured slowness is real.

## 10. Decisions log

| Date | Decision | Reasoning |
|---|---|---|
| 2026-05-08 | Waypoint transition controls reuse existing `BezierEasing` infra | The animation engine already supports per-call duration + easing curves; adding per-waypoint overrides is plumbing, not new infrastructure. |
| 2026-05-08 | Layer kinds are an enum on `DrawingProgramLayerListItem`, not separate types | Keeps the existing layer hierarchy intact; the kind is metadata on a normal layer. `DEFAULT` value preserves backward compat for InfiniPaint-format files. |
| 2026-05-08 | Auto-create named layers on new world; lazy-create on legacy load | Avoids forcing the user through a setup step; legacy files just get the layers added on first open. |
| 2026-05-08 | Sketch layer is raster-only; vector tools refuse it as edit target | Models the standard rough-then-ink pipeline cleanly. Sketch becomes a strict scratch surface — no vector pollution, no ambiguity about which layer the converted vectors land in. |
| 2026-05-08 | Three named layers (Sketch / Color / Ink), not two | The three-stage pipeline lets ink lines visually mask color bleed underneath, which removes the multi-color color-layer tracing problem entirely. Color stays raster forever; only the ink layer is a vectorization target. |
| 2026-05-08 | Top-bar uses a layer dropdown, not three pill toggles | A dropdown reads obviously as "pick a layer," scales if a fourth named layer ever lands, and uses much less top-bar real estate than three pills. Three pills would also crowd the phone layout. |
| 2026-05-08 | Vectorization is per-stroke replay, not pixel-tracing | Per-stroke recording at draw time gives perfect color preservation, perfect ordering, and pressure-aware variable-width output — and trivially handles multi-color ink (which our brushes support). Pixel-tracing always loses fidelity at corners and breaks down on multi-color. The cost is real (stroke log on disk + eraser must touch the log) but contained. |
| 2026-05-08 | Eraser fully removes touched strokes from the log | The simplest correct behavior. The rasterized partial erasure still shows correctly via tile dabs, but the convertibility of a partially-erased stroke is sacrificed. Geometric stroke splitting is deferred. |
| 2026-05-08 | Pre-Phase-2 raster cannot be vectorized | Stroke recording is opt-in via the format bump; older files have no log. The convert tool no-ops on those layers with a tooltip. New strokes drawn into a loaded-old layer record properly, so the layer becomes partially convertible. Avoids any "retroactive trace" complexity. |
