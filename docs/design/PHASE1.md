# HVYM Mischief — Phase 1 Design Doc

> **Audience:** the agent (and any human contributor) working inside the HVYM Mischief repo, which is a fork of [`ErrorAtLine0/infinipaint`](https://github.com/ErrorAtLine0/infinipaint).
>
> **Goal of this doc:** define what Phase 1 ships, where each new piece slots into the existing InfiniPaint codebase, and what is explicitly out of scope.

## 1. Product summary

HVYM Mischief is an infinite-canvas app for **producing and reading comics**. Phase 1 introduces two distinguishing features on top of the InfiniPaint base:

1. **Waypoints** — droppable canvas markers that capture camera state, panel framing, and a position in a comic's reading graph.
2. **A separate node/tree window** — a real second OS window for connecting waypoints into a graph that defines reading order and (optionally) branches.

Authoring and reading happen in the same app, switched via a mode toggle.

A third workstream lifts brush quality to natural-media feel for comic linework, scoped tightly to ink pens and markers.

## 2. Inheritance from InfiniPaint

InfiniPaint already provides:

- True infinite-zoom canvas, layers with blend modes, undo/redo
- Skia 2D drawing on a Vulkan surface, via SDL3
- Pressure-sensitive tablet input
- Existing tools: `BrushTool`, `EraserTool`, `LineDrawTool`, `RectDrawTool`, `EllipseDrawTool`, `TextBoxTool`, selection tools, eyedropper, pan, zoom
- **Bookmarks** ("Place bookmarks on the canvas to jump to later") — the data model HVYM waypoints extend
- Cross-platform builds: Windows / macOS / Linux / Android, plus web via Emscripten
- Conan-managed deps, CMake builds

Keep all of this. Phase 1 adds on top.

## 3. Repository orientation

Key paths the new work touches:

```
src/
  DrawingProgram/
    Tools/
      BrushTool.{cpp,hpp}         # existing Skia-path brush — keep, retitle as "Ink/Marker (vector)" in UI
      EraserTool.{cpp,hpp}        # touch only if libmypaint layers need erasing
      DrawingProgramToolBase.hpp  # base class — MyPaintBrushTool and WaypointTool inherit from this
    DrawingProgram.{cpp,hpp}      # tool registry; add MyPaintBrushTool + WaypointTool here
  CanvasComponents/
    BrushStrokeCanvasComponent.{cpp,hpp}  # existing path-stroke component — reference for new MyPaintLayerCanvasComponent
  Bookmarks/                      # existing — BookmarkManager + BookmarkListItem; supports folders + NetObj sync.
                                  # Becomes a compatibility shim around WaypointGraph; full removal in M5/M6.
  World.{cpp,hpp}                 # owns BookmarkManager today; gains WaypointGraph alongside, then loses BookmarkManager
  InputManager.{cpp,hpp}          # second-window input routing
  VulkanContext/                  # second-window surface management
conanfile.py                      # add libmypaint + json-c
```

New files Phase 1 introduces:

```
src/
  DrawingProgram/Tools/
    MyPaintBrushTool.{cpp,hpp}
    WaypointTool.{cpp,hpp}
  CanvasComponents/
    MyPaintLayerCanvasComponent.{cpp,hpp}
    WaypointCanvasComponent.{cpp,hpp}
  Brushes/
    LibMyPaintSkiaSurface.{cpp,hpp}     # MyPaintTiledSurface implementation
    BrushPresets.{cpp,hpp}              # curated ink/marker preset registry
    presets/*.myb                       # bundled brush preset files
  Waypoints/
    Waypoint.{cpp,hpp}                  # data model
    WaypointGraph.{cpp,hpp}             # nodes + edges, layout state
  TreeWindow/
    TreeWindow.{cpp,hpp}                # second SDL_Window; owns its own Skia surface
    GraphView.{cpp,hpp}                 # node-graph rendering and interaction
  ReaderMode/
    ReaderMode.{cpp,hpp}                # mode toggle, navigation, transitions
```

## 4. Workstream A — Brushes (libmypaint, scoped to ink + markers)

### Why

InfiniPaint's `BrushTool` strokes are `SkPath` with pressure-mapped width — clean, zoom-stable, but firmly "ink/marker" and lacking natural-media feel. For comic linework that's a usable baseline; for inkers wanting brush-pen taper, edge texture, and dab variation it's not enough.

### Approach

Add a **second brush tool** (`MyPaintBrushTool`) alongside `BrushTool`. The user picks per-stroke which they want. Existing `BrushTool` keeps working unchanged; do not modify its rendering path.

`libmypaint` (MIT-licensed C library, embedded as a brush engine in both Krita and GIMP) is added as a Conan dependency. A `LibMyPaintSkiaSurface` adapter implements `MyPaintTiledSurface` and composites libmypaint's 64×64 dab tiles into a Skia raster surface that lives inside a new `MyPaintLayerCanvasComponent`.

### Curated brush set (Phase 1)

Hide all libmypaint presets except a curated list. Approximately:

- Technical pen (uniform width, hard edges)
- Fine inker
- Brush pen (pressure-tapered)
- Fine marker
- Broad marker
- *(one slot reserved for tuning during integration)*

Wet/oily/watercolor/smudge/charcoal/bristle presets are **out of scope for Phase 1** — even though libmypaint can do them. The brush picker UI must not surface them.

### License compatibility

libmypaint (MIT) × InfiniPaint (MIT) × Skia (BSD-3) × SDL3 (zlib). All compatible. HVYM Mischief stays MIT.

### Phase 1 brush risks

- **Tile-model bridge.** libmypaint expects a 64×64 tile grid; canvas is logically infinite. Solve with lazy tile allocation tied to which tiles a stroke touches. Begin with a "hello dab" spike that drives a single dab onto a Skia surface to verify the bridge before integrating into a tool.
- **Raster vs. vector zoom stability.** libmypaint dabs are raster — strokes drawn at 1× zoom soften when the user zooms to 100×. Document this in the brush picker tooltip ("textured, fixed resolution") and let users pick the vector `BrushTool` when zoom-stability matters.
- **Emscripten/web.** libmypaint to WASM is non-trivial. Phase 1 ships native only; web build keeps working but the libmypaint tools are disabled in the web target.
- **Eraser interaction.** `EraserTool` currently understands `SkPath` strokes; deciding how it erases libmypaint raster tiles is a real design call. Likely answer: eraser becomes a destination-out dab pass on the raster surface for libmypaint layers, unchanged path-erase for vector strokes.

## 5. Workstream B — Waypoints

### Data model

Waypoints extend (and replace, on disk) InfiniPaint's existing bookmarks. The existing bookmark system uses `CoordSpaceHelper` paired with `WorldScalar` for true infinite-zoom precision, and is a deep `NetworkingObjects::NetObj*` participant — both of which the waypoint model preserves.

```cpp
struct Waypoint {
    NetObjID         id;
    std::string      label;
    CoordSpaceHelper coords;        // camera position/zoom/rotation, full WorldScalar precision
    Vector<int32_t,2> windowSize;   // matches BookmarkData; framing rect derives from this + coords
    std::optional<SkImage> thumbnail;  // small raster, regenerated lazily
};
```

A `WaypointGraph` owns:

```cpp
struct Edge {
    NetObjID                   from;
    NetObjID                   to;
    std::optional<std::string> label;  // shown as choice text in branching reader mode
};

struct WaypointGraph {
    NetObjOwnerPtr<NetObjOrderedList<Waypoint>> nodes;   // flat list, NetObj-synced
    NetObjOwnerPtr<NetObjOrderedList<Edge>>     edges;   // ordered per-source-node, NetObj-synced
    std::map<NetObjID, Vector2f>                layout;  // node positions in the tree window
};
```

The `WaypointGraph` participates in the multi-user `NetObjManager` so that both the canvas window and the tree window — and remote collaborators — see the same model. (This is technically an extension of collaboration beyond the doc's "do not extend" stance, but the alternative — silently dropping bookmark sync — was judged worse than preserving it.)

### Migration from existing bookmarks

On loading an old InfiniPaint file, each `BookmarkListItem` becomes a waypoint with no outgoing edges. Folder hierarchy is **flattened**: a bookmark inside `Chapter 1/Page 3/` becomes a waypoint with label `"Chapter 1 / Page 3 / <name>"`. Migration is lossless-ish — folder structure is recoverable from the prefix but not preserved as a tree. Author opts in to connecting waypoints with edges. Old files do not need a backwards-write path.

The existing `BookmarkManager` and its side-panel UI (`setup_list_gui`) are removed in M5/M6 once `WaypointTool` and the tree window provide equivalent navigation. Until then, `BookmarkManager` stays in place as a compatibility shim that round-trips through `WaypointGraph`.

### `WaypointTool` (canvas)

- Click on canvas → drop a waypoint at that point. Initial framing rect = current viewport.
- Selected waypoint shows handles to drag the framing rect (corners + sides).
- Click an existing waypoint → focus camera on its framing.
- Author mode: render markers with chrome (label, ID badge, faint outgoing edges).
- Reader mode: chrome hidden; only framing rect drives the camera.

### `WaypointCanvasComponent`

Mirror the structure of `BrushStrokeCanvasComponent`. Renders the marker, framing handles, and edge previews when selected.

## 6. Workstream C — Tree/graph window

A **real second OS window**, not a side panel. SDL3 supports multiple windows natively (`SDL_CreateWindow` again).

### What it shows

- One node per waypoint, rendered with thumbnail + label
- Edges as arrows between nodes; ordering of edges per source node defines reader-choice ordering for branches
- Manual node positioning persists in `WaypointGraph::layout`
- Auto-layout option (Sugiyama / layered) as a one-shot button — does not override saved positions until accepted

### Interactions

- Drag from a node's edge port to another node → create edge
- Right-click edge → delete or label
- Double-click node → focuses canvas window on that waypoint
- Selection sync: select in tree highlights on canvas, and vice versa

### Implementation choice — spike during this workstream

Two viable paths:

| Option | Pros | Cons |
|---|---|---|
| **`imnodes` (Dear ImGui addon)** | Fast to ship, well-tested node-graph UX, good defaults | Different rendering stack (ImGui) than the rest of the app |
| **Skia-rendered custom graph view** | Visual consistency with main canvas, full control over styling | More code; reinventing node-graph interaction |

Default to a one-day **imnodes** spike. If it lands cleanly and looks acceptable, ship with it. If branding/consistency matters more than ship-speed, switch to Skia after Phase 1.

### Window plumbing

`TreeWindow` owns:

- Its own `SDL_Window`
- Its own Skia surface bound to that window's swapchain
- Its own input loop dispatch (extend `InputManager` to multiplex by window ID)
- A pointer to the shared `WaypointGraph` (single source of truth — both windows mutate the same model)

## 7. Workstream D — Reader mode

A toggle (menu + keyboard shortcut) that:

- Hides editor chrome: toolbar minimized, waypoint markers hidden, tree window hides edge editing affordances (or auto-hides itself entirely — flip a coin during build, default to auto-hide)
- Anchors camera to the current waypoint's framing rect
- Forward/back keys (or pen-tap zones) traverse the graph
- Branch waypoints (multiple outgoing edges) surface a small choice UI overlay using the edge labels
- Smooth camera transitions between connected waypoints — pan + zoom interpolation, ~400ms default, configurable

Cycles and dead-ends are both legal: cycles let comics loop; dead-ends show a "the end" affordance. No editor warning either way — author's intent.

## 8. File format

Extend InfiniPaint's existing format:

- New top-level `waypoints` array (replaces `bookmarks`; old files migrated on load)
- New `waypoint_graph` object: `{ edges: [...], layout: {...} }`
- Per-stroke `brush_kind`: `"skia_path"` | `"mypaint"` (with embedded brush preset name + libmypaint settings hash)
- libmypaint layers serialized as compressed PNG tiles indexed by tile coordinate, alongside the existing vector strokes
- File format version bump. Old files load forward; new files do not load in upstream InfiniPaint (and we do not promise backward write compatibility).

## 9. Out of scope for Phase 1

Real things the app might want — explicitly **not now**:

- Wet media / oil / watercolor / smudge / charcoal brushes
- Custom brush authoring UI
- Multi-user collaboration (InfiniPaint has it; keep working but do not extend; revisit later)
- Animation / timeline
- Comic-format export (.cbz, .pdf-as-comic, etc.)
- ASDF-quality vector zoom (research project; possibly never)
- Mobile-first UI (Android target keeps building but isn't a Phase 1 UX target)
- Cloud sync, accounts, store

## 10. Milestones

Tracked as harness tasks #2 through #9 (#1 — the fork itself — is being done by the project owner outside this repo).

| # | Status | Milestone | Deliverable |
|---|---|---|---|
| M1 | ✅ | libmypaint plumbed | Conan dep added; "hello dab" test draws to a Skia surface |
| M2 | ✅ | LibMyPaintSkiaSurface adapter | `MyPaintTiledSurface` implementation with lazy tile allocation; unit-tested against synthetic stroke input |
| M3 | ✅ | MyPaintBrushTool + curated presets | New tool registered in `DrawingProgram`; brush picker shows the curated set; eraser interaction defined |
| M4 | ✅ | Waypoint data model | `Waypoint`, `WaypointGraph`; ~~bookmark migration~~; file format updated; round-trip tested (manual) |
| M5 | ✅ | WaypointTool + canvas component | Drop / edit / delete waypoints on canvas; ~~framing rect handles~~ (deferred); author chrome (label, framing outline, edge previews); reader chrome with M7 |
| M6 | | Tree window | Second SDL_Window with imnodes (or Skia) graph view; bidirectional sync with canvas |
| M7 | | Reader mode | Mode toggle, hidden chrome, camera transitions, branching choice UI |
| M8 | | Phase 1 release | Rebrand (name, icons, splash, About, .hvym extension); installers; release notes |

**M3 follow-ups deferred:** persistent tile serialization (rolls into M4 file format); real `.myb` file loading (current presets are hardcoded `apply()` functions; the public BrushPreset shape is stable across that swap); per-brush pressure-sensitivity slider in the picker (currently only diameter/hardness/opacity are user-tunable).

**M4 scope cuts:** Bookmark migration and the §5 "BookmarkManager-as-compatibility-shim" are both descoped — this codebase is not promising to load anyone else's pre-0.5 InfiniPaint files. M5 can therefore remove `BookmarkManager` outright when it replaces the side-panel UI, rather than maintaining a shim.

Workstreams A (brushes: M1–M3) and B+C (waypoints + tree: M4–M6) run in parallel after the fork is built clean. M7 depends on M5 + M6. M8 depends on M3 + M7.

## 11. Open product questions

Not blocking — flag and decide as Phase 1 progresses.

- **Branching depth.** Mostly-linear comics with occasional branches, or full choose-your-own-adventure trees? Affects how prominent branching UI is.
- **Waypoint granularity.** One waypoint per panel, or one per page-with-many-panels? Current design assumes per-panel.
- **Reader mode transition style.** Pan, zoom-out-zoom-in, fade. Probably configurable; pick a default.
- **Tree window: imnodes vs. Skia.** Spike during M6.
- **Multi-user collaboration.** Keep InfiniPaint's lobby feature working as-is, deprecate it, or extend it to share waypoint state? Default: keep working, do not extend.

## 12. Local dev environment

Conventions for working in this repo on the maintainer's machine. Not user-facing; record here so future contributors and the agent don't drift.

- **Python tooling: use `uv`.** All Python invocations — Conan itself (it's a pip package), helper scripts, anything that would otherwise be `python …` or `pip install …` — go through `uv` (`uv run`, `uv tool install`, `uv pip`). Avoids polluting the system Python and gives reproducible per-task envs. Do not introduce raw `pip install` or bare `python` calls in docs, scripts, or CI snippets without a reason.
- **Conan cache location: `D:\.conan2`.** Set via the user-scope `CONAN_HOME` environment variable. The repo lives on `D:\` because `C:\` was running out of space; the Conan cache (which can grow to many GB across Skia + dependency builds) must live on `D:\` for the same reason. New shells inherit this automatically; if a tool reports the cache as `C:\Users\...\.conan2`, the shell pre-dates the env var and needs to be reopened.

## 13. Decisions log

| Date | Decision | Rationale |
|---|---|---|
| 2026-05-07 | Base on InfiniPaint | C++/MIT/Skia/Vulkan, working infinite canvas, layers, tablet, bookmarks — closest existing match |
| 2026-05-07 | Brush engine: libmypaint, scoped to ink+markers only | Best-in-class FOSS natural-feel brushes; scoping limits integration surface and matches comic use case |
| 2026-05-07 | Two-tool brush model | Keep `BrushTool` (vector, zoom-stable) for ink lines; add `MyPaintBrushTool` (raster, textured) for natural-media feel; user picks per stroke |
| 2026-05-07 | Hard fork, no upstream tracking | Phase 1 changes are too invasive for clean upstream sync; revisit if it becomes valuable |
| 2026-05-07 | Waypoints replace bookmarks on disk | Lossless one-way migration; cleaner data model than dual-typing |
| 2026-05-07 | Tree window is a real second OS window | SDL3 supports it natively; matches user spec; better multi-monitor story than a panel |
| 2026-05-07 | Waypoint stores `CoordSpaceHelper`, not `Vector2f`+float | Existing bookmarks use `CoordSpaceHelper` + `WorldScalar` for true infinite-zoom precision. Vector2f+float would silently break waypoints at extreme zoom. |
| 2026-05-07 | Waypoints participate in NetObj sync | Bookmarks today are NetObj-synced; preserving that means waypoints must be too. Counts as extending collaboration despite §9, but silently breaking bookmark sync was judged worse. |
| 2026-05-07 | Folders flattened into label prefixes on migration | Doc's flat `WaypointGraph::nodes` matches the user's choice. Folder names survive as `"Folder / Sub / Name"` prefixes; structure is recoverable, not preserved. |
| 2026-05-07 | Bookmark side panel removed; tree window replaces it | Single navigator. `BookmarkManager` stays as a thin compat shim until M6 lands the tree window, then is removed. |
| 2026-05-07 | File extension stays `.infpnt` until M8 rebrand | Format version bumps in M4 (so old files can load forward), but the user-visible `.hvym` rename happens with the rest of the rebrand in M8. |
| 2026-05-07 | Reader mode is per-session, not per-document | Default; can revisit if authors want per-comic preview state. |
| 2026-05-07 | libmypaint integrated as `deps/libmypaint` git submodule, not a Conan recipe | libmypaint is autotools-only and not on stock ConanCenter; writing an MSVC-clean Conan recipe is 4–8h of yak-shaving before any brush code runs. Matches existing `deps/clip` precedent. Krita/GIMP both vendor libmypaint for the same reason. Revisit if the spike grows into a maintenance burden. |
| 2026-05-07 | Repo and Conan cache on `D:\` instead of `C:\` | `C:\` was running out of space; Conan cache can balloon to many GB during Skia builds. `CONAN_HOME=D:\.conan2` set as user env var. |
| 2026-05-07 | `uv` is the only Python entrypoint | Avoids system-Python pollution and version drift; gives reproducible per-task envs. Applies to Conan, helper scripts, and anything else Python-shaped. |
