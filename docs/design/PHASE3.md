# Inkternity — Phase 3 Design Doc

> **Audience:** the agent (and any human contributor) working inside the Inkternity repo.
>
> **Goal of this doc:** define what Phase 3 ships on top of the Phase 1 + 2 feature set + the DISTRIBUTION-PHASE1 hosting/identity layer, where each new piece slots into the existing Inkternity + InfiniPaint codebase, what is in scope, and what is explicitly out of scope.
>
> **Status:** early draft — workstreams and decisions are still being hashed out. Open product questions section is where unresolved scope sits; Decisions log captures choices as they're made.

## 1. Product summary

Phase 3 sharpens Inkternity into a tool that **feels personal to each artist** rather than a one-size-fits-all preset surface. Two initial workstreams:

1. **Brush customization + saved-preset library**, split across two purpose-built drawers:
   - *Customization drawer* (icon: brush + compass, "navigate the brush") — exposes the full set of libmypaint parameters on the active brush so the artist can tune for the right feeling. Includes a **capture-from-canvas icon flow** (the same primitive used for Phase 1 waypoint skins, with the selection rect locked to 1:1 aspect): the artist draws a test stroke with their tuned brush, drags a square around it, and that captured 64×64 image becomes the saved preset's icon.
   - *Saved presets drawer* (icon: brush library) — a glance-and-pick surface for browsing the curated + user-saved presets as a grid of artist-drawn icon tiles, with search / drag-reorder / context menus for rename / edit / delete / duplicate.
   - Custom presets live in `<configPath>/brush_presets/`, one JSON + one PNG sidecar per preset.

2. **Artist avatar — drawn on canvas, shown in the top toolbar + on remote cursors.** Collab sessions today render each remote player's cursor as a colored circle. Phase 3 lets each artist *draw* their avatar on the canvas (reusing the same square-locked capture primitive Workstream A introduces for brush icons), display it as a profile-image tile in the canvas-view top toolbar, and broadcast a 64×64 wire form to collab peers so SUBSCRIPTION-host and collab sessions feel like *people drawing*, not anonymous color blobs. Local store is 256×256 (room for future higher-DPI displays + portal-side profile uses); wire payload is 64×64 (negligible bandwidth, one broadcast per peer per join).

Phase 3 ships under the same `.inkternity` extension. The customization + avatar artifacts live in the user-config area (next to `inkternity_dev_keys.json`), not in canvas files — they travel with the artist, not the work.

## 2. Inheritance from Phase 2 and DISTRIBUTION-PHASE1

Phase 3 builds on:

- **libmypaint integration** (Phase 1 M1–M3, refined in Phase 2) — `MyPaintBrushTool`, `LibMyPaintSkiaSurface`, the curated `BrushPresets`. Brush customization extends this by replacing "pick from curated set" with "pick from curated *or* your own presets, and tweak any active brush live."
- **Per-stroke recording** (Phase 2 M1) — already records color + base radius + samples on `MyPaintLayerCanvasComponent`. No expansion needed for Workstream A; the recording fields remain unchanged.
- **NetObj `ClientData` sync** (Phase 0) — already carries cursor color + display name across collab. Workstream B extends `ClientData` with an avatar payload (small PNG bytes); the existing sync path delivers it.
- **Persistent app identity** (DISTRIBUTION-PHASE1.md §3) — the artist's `G...` keypair is now stable across reinstalls. Avatars (and custom brush presets) live next to that keypair in the user-config dir, so reinstalling Inkternity doesn't lose them.
- **Wire-side raster sync** (RASTER-WIRE-SYNC.md) — libmypaint strokes now replicate end-to-end. Customized brush presets used during a collab will render correctly on the remote side because the on-the-wire payload is tile data, independent of the brush parameters that produced it.

## 3. Workstream A — Brush customization + saved-preset library

### Why

The Phase 1 + 2 brush surface gives the artist a curated set of libmypaint presets organized into two categories (SHARP / TEXTURED in `BrushPresets`). That's enough to draw with, but every libmypaint brush has ~50 tunable parameters (radius, hardness, opacity, smudge, dabs-per-radius, tracking, speed mapping, pressure mapping, color modulation, etc.) and individual artists have strong preferences. The current path to "I want my pen to feel slightly softer at low pressure" is: edit the hardcoded `apply()` function in `BrushPresets.cpp`, rebuild Inkternity. That's not an artist workflow; that's a code workflow.

Phase 3 makes brush feel a first-class artist concern split across two surfaces:

- **A1 — Customization drawer**: tune the live brush's libmypaint params, capture a custom icon for it by drawing on the canvas, save the result as a named preset.
- **A2 — Saved presets drawer**: browse and switch between the curated + user-saved presets, each with its own icon (artist-drawn for user presets, bundled SVG for curated).

The split mirrors how an artist actually works: *building* a brush (rare, careful, deliberate) is a different action from *picking* a brush (frequent, fast, glanceable).

### A1 — Customization drawer

#### Surface shape

A new toolbar button (icon: `data/icons/live-brush.svg` — zynx is authoring this; ship a placeholder copied from any existing icon under that filename so the swap-in is a single-file commit). Clicking opens a **right-side drawer**, mirroring the layer-manager panel from Phase 2. The drawer is canvas-aware: when an HVYM MyPaint tool is active, it shows the live params of that brush; when a vector tool is active, the drawer is hidden or shows a "no MyPaint brush active" placeholder.

**Single-drawer rule (this and A2 and the layer manager):** only one right-side drawer is open at a time. Toggling open one auto-closes any other. Keeps the canvas unobstructed and avoids visual competition on narrower windows.

#### Drawer layout

Four regions, top to bottom:

```
+----------------------------------+
| Preset: My Inky Pen 3   [▼]      |  <- preset selector + dropdown
| Category: Ink           [Save]   |  <- where this preset will save to
+----------------------------------+
| Icon: [ 64x64 preview ]          |  <- artist-drawn icon for this preset
|       [ Capture from canvas... ] |  <- enters icon-capture mode (see below)
|       [ Clear ]                  |
+----------------------------------+
| ☰ Basic                          |  <- collapsible group
|   radius_logarithmic   [——|——]   |
|   hardness             [—|———]   |
|   opacity              [——|——]   |
|   ...                            |
| ☰ Speed                          |
|   speed1_slowness      [—|———]   |
|   ...                            |
| ☰ Smudge                         |
|   smudge               [——|——]   |
|   smudge_radius        [—|———]   |
|   ...                            |
| ☰ Pressure mapping (advanced)    |
|   (collapsed by default)         |
+----------------------------------+
| [Reset all]   [Save as preset]   |
+----------------------------------+
```

Each parameter row: name (or short ID) + numeric input + slider clamped to libmypaint's documented sensible range. Hovering shows the full parameter description from libmypaint's brush-settings JSON. Changes apply to the live brush immediately — no "apply" button — and persist until the artist saves or switches presets.

Pressure-mapping (and other input curves: speed, random, stroke) is **collapsed by default** in v1 because curve editing is a heavyweight UI in its own right. Phase 3 ships a scalar value per input slot; full curve editor is deferred to a future polish phase.

**Color parameters are deliberately excluded from saved presets.** A brush preset captures *feel* (radius, hardness, opacity, smudge, pressure response, dab spacing) — never color. The artist's color picker is the sole authority on what color a stroke ends up: switching brush preset never changes the active color, and saving a preset never bakes in a color. Concretely, the libmypaint params with prefix `color_*` (base HSV) and `change_color_*` (per-dab color modulation) are filtered out of `MyPaintBrushParams` so they never appear in the drawer and aren't serialized to preset JSON. If a future polish wants brush-driven color effects (e.g. a brush that intentionally shifts hue per dab as part of its feel), that's a separate decision; for v1 the boundary is hard.

#### Custom brush icons — capture from canvas

The "Capture from canvas..." button is the original idea here, lifted from Phase 1's `ButtonSelectTool` waypoint-skin capture flow: the artist draws *with the brush they're tuning* on the canvas (whatever feels representative — a single thick stroke, a swatch test, anything), then drags a selection rect around it and the captured pixels become the preset's icon.

Constraint: **the selection rect is locked to a 1:1 aspect ratio.** When the artist drags, the rect grows uniformly from the anchor point along whichever axis is dominant — releasing the mouse always commits a perfect square. Square-locked selection is a deliberate tool variant of `ButtonSelectTool`; cleanest to fork the tool into a sibling (`BrushIconCaptureTool`) rather than overload the existing one with an aspect-lock flag.

Captured pixels are downscaled to a uniform 64×64 PNG on save (regardless of capture zoom level) so every preset's icon is the same dimensions in the saved-presets drawer. The PNG is stored as a sibling file next to the preset JSON: `<configPath>/brush_presets/<category>/<name>.icon.png`. Sibling files (not inline base64) keep the JSON human-readable and let artists hand-replace an icon with an image editor if they want to.

**Selection-rect max size:** the square-locked selection rect caps at 2048 source pixels per side. An artist dragging beyond that freezes the rect at the cap. Keeps captures sane (well above what any 64×64 or 256×256 downscale needs) and avoids the pathological case of an artist trying to capture the entire viewport at extreme zoom-out. No soft-warn for "this looks blurry" — just capture; if it looks weird the artist re-captures. Keep it simple.

If the artist doesn't capture an icon, the preset saves without one and the saved-presets drawer renders a **hash-derived placeholder** — a small generated tile whose colors come from hashing the preset name (so the same name always gets the same placeholder; different names look different at a glance). Same placeholder is used if the icon PNG sidecar is missing for any reason (artist hand-deleted it, filesystem corruption, etc.) — preset still loads, just renders with the placeholder. If no easy hash-to-icon library drops in cleanly, ship a single generic `data/icons/brush_presets/_no_icon.png` as the universal fallback.

#### Preset persistence

User presets live at `<configPath>/brush_presets/<category>/<name>.json` (params) + `<configPath>/brush_presets/<category>/<name>.icon.png` (icon, optional). Param JSON mirrors libmypaint's `.myb` JSON-ish format (param → value) so future "import from external `.myb` file" is a transparent operation. Categories are subdirectories.

At startup, Inkternity scans `<configPath>/brush_presets/**/*.json` and registers them alongside the curated presets. Both flow into the same `BrushPreset` registry the existing `MyPaintBrushTool` consumes.

Curated presets remain hardcoded in `BrushPresets.cpp` — they're the "ship-quality" defaults. Their icons ship in `data/icons/brush_presets/<name>.png` (PNG matches the artist-captured-PNG medium for user presets, so the A2 grid renders both kinds with a single image path).

Treat the existing curated set as a **native / legacy set** for Phase 3: the inline picker row in `gui_toolbox` / `gui_phone_toolbox` (`render_brush_picker_row`) stays as-is. A2 surfaces these same curated presets in the new drawer alongside user presets; both surfaces co-exist this phase. The decision to consolidate (drop the inline row, route picking through A2 only) is deferred.

**Duplicate-to-edit is available** from A2's per-preset context menu (A2.M3 below). Right-click any curated preset → Duplicate → a new user preset is cloned into the user-presets dir with the same params and icon, the artist edits the copy. Curated presets stay read-only; A1's Save button on a curated-preset-active brush triggers Save-as-new (not overwrite).

#### Live editing semantics

When the drawer is open:

1. Adjust a slider → `MyPaintBrushTool` config is mutated immediately
2. Next pen-down uses the new params
3. In-progress stroke is unaffected (libmypaint reads params at `begin_atomic`; we don't re-read mid-stroke)
4. Closing the drawer doesn't reset params — the live brush stays "dirty" until the artist explicitly resets or saves
5. Switching tools or canvases doesn't reset params either; the drawer's state is per-active-brush-preset, not per-tool-invocation

Save flow:
- "Save as preset" → modal with name + category text inputs
- On confirm, writes the new JSON to `<configPath>/brush_presets/<category>/<name>.json` + (if captured) the icon to `<name>.icon.png`, refreshes the saved-presets drawer, makes the new preset the active selection
- "Save" (over existing user preset) writes back to the same file pair

### A2 — Saved presets drawer

#### Why a separate drawer

The customization drawer is heavy and parameter-dense; the artist wants it open only when they're *building*. Day-to-day picking a brush is a fast operation: glance at the icons, click one. The current brush-picker popup inside the tool settings is fine for the curated 8-or-so brushes but doesn't scale to "your 30 custom presets + the curated set." A dedicated drawer gives the picking action room to breathe and makes the artist's saved library a visible part of the workspace.

#### Surface shape

A second new toolbar button (icon: `data/icons/brush-library.svg` — a basic library glyph; can be sourced from an icon set or composed). Clicking opens its own right-side drawer. Per the single-drawer rule (above), opening A2 closes A1 (and vice versa); the artist switches between them rather than running both at once.

#### Drawer layout

Grid of icon tiles, grouped by category:

```
+----------------------------------+
| [search...]                      |
+----------------------------------+
| Curated — Ink                    |
| [tech] [fine] [brush] [wet]      |  <- 4 icons in a row, name on hover
| Curated — Pencil                 |
| [smooth] [textured] ...          |
| Your presets — Ink               |
| [icon] [icon] [icon] [+]         |  <- artist-drawn icons; [+] is "new from
|                                    current customization drawer state"
| Your presets — Sketch            |
| [icon] [icon] ...                |
+----------------------------------+
```

Tile actions:
- **Click**: make this preset the active brush. The MyPaint tool switches to it; the customization drawer (if open) updates to show its params.
- **Long-press / right-click**: per-preset menu — Rename, Edit (opens customization drawer with this preset loaded), Delete (user presets only; curated are read-only), Duplicate (clones into Your presets so the artist can edit a copy).
- **Drag**: reorder within a category or move between categories (user presets only).

The search field filters by name across all categories.

#### How A1 and A2 work together

- Open A2 → click a preset → that preset is now active. Close A2.
- Open A1 → params reflect the active preset. Tune sliders → live brush is dirty.
- Save in A1 → either overwrites the active user preset (if it was a user preset) or opens the "Save as preset" modal (if active was a curated, which is read-only). New saves immediately appear in A2.
- Capture icon in A1 → captured square attaches to the in-progress preset. Saved on next Save.

### Surfaces touched

```
src/Brushes/BrushPresets.{hpp,cpp}    expand BrushPreset struct to include all
                                       libmypaint params (not just the few
                                       curated ones) and an optional iconPath.
                                       Refactor apply() to set every param
                                       from the struct rather than hardcoding
                                       6-8 of them.

NEW src/Brushes/MyPaintBrushParams.{hpp,cpp}
                                       canonical list of libmypaint param IDs +
                                       sensible-range metadata + display
                                       grouping. Generated from libmypaint's
                                       brushsettings-gen.py output (or hand-
                                       maintained — only ~50 entries).

NEW src/Brushes/UserBrushPresets.{hpp,cpp}
                                       scan_dir / save / load / delete helpers
                                       for <configPath>/brush_presets/.
                                       Handles the param-JSON + icon-PNG
                                       sidecar pair as one preset unit.
                                       Mirrors PublishedCanvases module shape.

src/DrawingProgram/Tools/MyPaintBrushTool.{hpp,cpp}
                                       add live-param mutators so the
                                       customization drawer can write back
                                       to the active brush config without
                                       reaching through BrushPresets.

NEW src/DrawingProgram/Tools/BrushIconCaptureTool.{hpp,cpp}
                                       sibling of ButtonSelectTool (Phase 1
                                       M-skin). Tool activates from the
                                       customization drawer's "Capture from
                                       canvas..." button, takes over the
                                       cursor with a square-locked select
                                       rect, on release captures pixels +
                                       downscales to 64x64 PNG + binds to
                                       the in-progress preset state. Restores
                                       the prior tool when done or canceled.

NEW src/Toolbar/BrushCustomizationDrawer.{hpp,cpp}
                                       the A1 drawer UI. New top-toolbar icon
                                       button to open/close. Layout via the
                                       existing Clay panel pattern (mirrors
                                       layer-manager drawer from Phase 2).

NEW src/Toolbar/SavedPresetsDrawer.{hpp,cpp}
                                       the A2 drawer UI. Its own top-toolbar
                                       icon button. Grid-of-tiles layout.
                                       Search filter, drag-reorder, context
                                       menu actions (Rename, Edit, Delete,
                                       Duplicate).

NEW data/icons/live-brush.svg          toolbar icon for the A1 drawer.
                                       zynx is authoring; placeholder copied
                                       from any existing icon under this
                                       filename until then.
NEW data/icons/brush-library.svg       toolbar icon for the A2 drawer.
                                       Basic library glyph; can be sourced
                                       from an icon set.
NEW data/icons/brush_presets/*.png     per-curated-preset icons for the
                                       built-in brushes (tech-pen, fine-ink,
                                       brush-pen, wet-ink, etc.). One PNG
                                       per curated preset, matching the
                                       artist-captured-PNG medium for user
                                       presets so the A2 grid renders both
                                       kinds with a single image path.
NEW data/icons/brush_presets/_no_icon.png
                                       generic fallback for user presets
                                       that have no captured icon AND when
                                       hash-derived placeholder generation
                                       isn't viable.

src/Toolbar.cpp (desktop) +
src/Screens/PhoneDrawingProgramScreen.cpp (phone)
                                       wire BOTH new toggle buttons in both
                                       canvas-view top toolbars (lesson
                                       learned from DP1-B: phone is its own
                                       UI surface, port everything).
```

### Risks

- **libmypaint param count is large.** ~50 base params + ~5 inputs each = 250+ widgets if we exposed everything. Phase 3 ships scalar-only (no curve editing); deferring curves is the main complexity reduction.
- **Param value ranges aren't always documented.** Some libmypaint params have weird sensible ranges (e.g., negative slownesses, infinite multipliers). Need to read the brushsettings JSON or hand-curate ranges. Risk: a slider that lets the artist set a value that crashes libmypaint or produces invalid strokes.
- **Curated presets stay hardcoded → schema drift.** If `BrushPresets.cpp` and user-preset JSON drift apart, the brush picker shows inconsistent capabilities. Mitigation: same `BrushPreset` struct + same `apply()` function for both; user presets are JSON-loaded into the struct.
- **Icon capture's square constraint on an arbitrarily-rotated/scaled camera.** `ButtonSelectTool` captures rect aligned to *canvas* axes, regardless of camera rotation. The square constraint applies in canvas-pixel space, which is what we want — the icon is always captured upright. Camera rotation doesn't enter the picture. (If we ever change ButtonSelectTool to capture in camera-space, we'd need to revisit.)
- **Forgotten icon → silent placeholder.** If the artist saves a preset without capturing an icon, A2 renders a placeholder. That's fine, but worth making the placeholder visually distinct so the artist knows "this preset has no icon yet" rather than "I picked an unflattering icon."
- **Two drawers, one canvas.** Both drawers and the layer manager could in principle all be open at once. Visually competing. Mitigation: drawers auto-close when another opens (single "active drawer" rule), or stack on the same side. Pick during implementation.
- **Custom brush params not synced over collab.** Per the existing brush-sync model — a custom preset used to paint a stroke is invisible to remote, but the *resulting stroke* sync via RASTER-WIRE-SYNC.md tile data is fine. The artist may want to share a preset (icon + params) with a collaborator; that's preset import/export (out of scope for v1, possible future).
- **Drawer UI on phone.** Side drawers are awkward on narrow screens. Phone may need a bottom-sheet or full-screen-modal variant for both A1 and A2. Defer concrete UI to phone parity pass during implementation.

## 4. Workstream B — Artist avatar (drawn on canvas, shown in top toolbar + on remote cursors)

### Why

In a collab session today, every remote artist's cursor is a colored circle plus a display name floating nearby (see `World::draw_other_player_cursors`). The color is randomly generated at first ClientData construction. The result is functional but anonymous — three people in a session are "red", "green", "blue" and the artist has to remember which color is which name. Avatars give every collaborator a glanceable visual identity that scales beyond three peers.

Importantly, Phase 3 picks up the **draw-it-on-the-canvas** pattern established by Workstream A's brush-icon capture (which itself inherits from Phase 1's waypoint-skin capture). The same primitive — square-locked canvas selection → downscale to PNG → bind to a destination — powers both surfaces. The artist's avatar isn't a stock image picker; it's something they draw with their tools, the same way they draw everything else in Inkternity.

### Surface shape — top toolbar profile image

The avatar lives **inline in the canvas-view top toolbar**, like a profile image in a social app. Far right of the bar, square tile (~32px on screen, scales with `finalCalculatedGuiScale`). Click → small popover with two action buttons over the image:

```
        ┌───┐ ┌───┐
[avatar][cap] [file]   <- popover over the avatar tile
        └───┘ └───┘
```

- **[cap]** — capture from canvas (active in Phase 3; mirrors the brush-icon capture flow)
- **[file]** — choose from file (button present but **disabled** in Phase 3; reserved for a future polish phase)

When no avatar is set, the tile renders as a small placeholder (a colored swatch derived from the artist's `G...` pubkey hash — same hue logic as the existing cursor color, just larger).

### Drawing and capturing the avatar

Clicking **[cap]** activates the canvas-capture flow:

1. Tool switches to the same square-locked select primitive Workstream A uses for brush icons (likely a unified `SquareCanvasCaptureTool` — see "Surfaces touched" below). The expected workflow: the artist has already drawn something they want as their avatar on the current canvas (a self-portrait, a sigil, a doodle); they just drag a square around it.
2. On release, the selected canvas region is captured at the actual pixel resolution it occupies on-screen.
3. The captured pixels are downscaled to **256×256** for the local "high quality" store at `<configPath>/avatar.png`.
4. The same 256×256 master is further downscaled to **64×64** for any network broadcast.
5. The top-toolbar tile + remote-cursor renderings update immediately for the artist; the next collab join broadcasts the new 64×64 wire form to peers.

The 256/64 split matters because the toolbar tile (and potentially future portal-side profile views) wants more pixels than the in-canvas cursor overlay does. Keeping the high-res master lets us produce a bigger render when context allows, while keeping the wire payload tiny.

### Storage and identity binding

Two files, both in the user-config dir next to `inkternity_dev_keys.json`:

```
<configPath>/avatar.png        local high-res master (256×256, ~10-30 KB)
<configPath>/avatar_wire.png   wire payload (64×64, ~3-8 KB)
```

The wire-form file is a precomputed downscale so we don't re-resize on every `ClientData` broadcast. Regenerated on every capture.

One avatar per app install (per app keypair). Reinstalling Inkternity from the same Stellar seed *won't* restore the avatar (it's not part of the seed-derived identity); the artist would re-capture it after a Restore App Key. That's intentional — avatars are local preference, not crypto-recoverable identity.

Future option (out of scope for Phase 3): publish the avatar to the portal so subscriber-facing artist branding includes it. Phase 3 keeps it strictly collab-local.

### Network sync

Add a `std::optional<std::vector<uint8_t>> avatarPng` field to `ClientData`, populated from `<configPath>/avatar_wire.png` (the 64×64 form). On join, each client broadcasts its `ClientData` (existing path); the new field rides along. Total wire overhead: one ~3-8 KB push per peer per join, never re-sent during a session. Negligible.

If the avatar is absent (artist never captured one), the field stays empty and the receiver falls back to the existing colored-circle rendering. No protocol break: pre-Phase-3 peers ignore the optional field; post-Phase-3 peers handle the empty case identically.

### Rendering

Two display contexts:

1. **Top-toolbar tile** (local artist viewing their own avatar): render the 256×256 master scaled down to the toolbar tile size with high-quality filtering.
2. **Remote cursor overlay** (other artists' cursors during collab): `World::draw_other_player_cursors` composites the remote's decoded 64×64 avatar at the cursor location, with the colored circle behind it as a fallback edge indicator. The cached decoded image lives on `ClientData` on the receiver side so we don't re-decode per frame.

**Subscriber-visibility rule.** A working artist hosting a SUBSCRIPTION canvas shouldn't see subscriber cursors+avatars cluttering their canvas — subscribers are passive viewers, not collaborators, and visible cursors+avatars per-subscriber would interfere with the artist's painting once a popular canvas has more than a couple of subscribers. Rule: when a remote `ClientData` is flagged `isViewer == true` (P0-D5), do NOT render its cursor or avatar on the host's view. COLLAB peers (non-viewer) render normally. This also implies the avatar is hidden in reader mode, since reader-mode users *are* viewers from the host's perspective.

### Surfaces touched

```
src/ClientData.{hpp,cpp}              add avatarPng field (64×64 wire form);
                                       serialize in save()/load(); add
                                       decoded SkImage cache + accessor for
                                       the renderer.

src/World.cpp::draw_other_player_cursors
                                       if remote has avatar, composite it.
                                       Keep colored-circle as fallback when
                                       avatar is empty or decode fails.

src/Toolbar.cpp (desktop) +
src/Screens/PhoneDrawingProgramScreen.cpp (phone)
                                       new far-right top-toolbar widget:
                                       avatar tile + click-to-open popover
                                       with [cap] and [file] buttons.
                                       Phase 3: only [cap] is wired;
                                       [file] renders disabled with a
                                       tooltip ("future phase").

NEW src/AvatarStore.{hpp,cpp}         load/save helpers for the two PNGs.
                                       Handles 256×256 master + 64×64 wire
                                       form regeneration on every capture.

NEW src/DrawingProgram/Tools/SquareCanvasCaptureTool.{hpp,cpp}
                                       UNIFIED capture primitive shared by
                                       Workstream A's BrushIconCapture
                                       (A1.M6) and this workstream's avatar
                                       capture. Sibling of ButtonSelectTool
                                       with the aspect-lock baked in.
                                       Constructor takes a target size +
                                       destination callback so callers can
                                       parameterize (64×64 for brush icons,
                                       256×256 master for avatars). Replaces
                                       the BrushIconCaptureTool stub
                                       described in A1.M6 -- one tool, two
                                       call sites.

(Settings → Identity NO LONGER gains an avatar row.)
                                       Discoverability is via the top-bar
                                       tile, not buried in Settings. The
                                       Settings Identity section stays
                                       focused on the cryptographic identity
                                       (G..., Export, Restore) per
                                       DISTRIBUTION-PHASE1.md §3.3.
```

### Risks

- **UI system support for "image with overlay buttons" in the top toolbar.** Per the decision below, this gets verified before the B.M2 / B.M3 milestones are scheduled — a small Clay spike confirms an interactive child popover can render over an image element with click hit-testing on the children. If it can't, fallback is a regular click handler that opens a small modal positioned near the avatar tile (less elegant, same functional result).
- **Top-toolbar real estate.** Top bar is already populated (tabs, menus, undo/redo, grid/layer/bookmark, the inline filename rename added in DP1-B polish). Far right is the natural slot; the avatar tile is square + small (~32px) so it competes only with whitespace. Phone top bar is more constrained — phone avatar tile may need to live in the main menu popover rather than inline.
- **Avatar drawn at low zoom looks blurry as 256×256.** If the artist captures from a heavily zoomed-out canvas, the source pixel rect is tiny and the upscale to 256 is mush. Mitigation: warn (or refuse) if the captured region is smaller than ~128 source pixels per side. Or accept the blur and let the artist re-capture at higher zoom.
- **Wire bandwidth on busy sessions.** A 5-peer collab session = 5 × ~8 KB avatar broadcasts on join ≈ 40 KB at peak. Negligible.
- **PNG decode robustness.** Avatars come from canvas pixels (Phase 3) or a file picker (deferred). Canvas-sourced PNGs are trustworthy because we encoded them ourselves; future file-picker support will need the same defensive-decode treatment as any user-provided image.
- **Inappropriate content via avatars.** Subscriber-facing artists can in principle draw whatever they want. SUBSCRIPTION mode hosts decide who joins their canvas (via portal token); the surface for "report avatar" is portal concern, not Inkternity. Document expectation; no in-app moderation.
- **Avatar privacy.** Some artists may not want a recognizable image attached to their cursor on every remote canvas. Default = no avatar (keep colored circle). Setting an avatar is explicit; removing it is one click on the tile's popover (add a [Clear] button to the popover alongside [cap]/[file]).

## 5. Workstream C, D… — open slots

Reserved for additional Phase 3 features as they're scoped. Candidate ideas to draft if/when they get prioritized:

- **Brush stabilizer / smoothing** — common request in digital painting; libmypaint's `tracking_noise` + `slow_tracking` already do part of this, but a dedicated stabilizer with adjustable strength is friendlier than tuning two raw params.
- **Custom keybinds for tool switching** — the existing keybind system covers some but not all tool slots; phase 3 could complete it.
- **Theme builder** — let artists author UI themes the way they author brush presets, with a live preview.
- **Per-canvas tag / category metadata** — for artists with many canvases, a way to organize beyond filename.

None of these are in scope until they're added with their own §X.

## 6. File format

Phase 3 doesn't touch the `.inkternity` canvas file format. All new persistent artifacts live in the user-config dir:

- `<configPath>/brush_presets/<category>/<name>.json` (Workstream A)
- `<configPath>/avatar.png` (Workstream B)

`ClientData` gains a new `avatarPng` field; that's a NetObj wire-format addition, not an on-disk addition. Pre-Phase-3 builds will ignore the optional field, so cross-version collab degrades gracefully (older peers see colored-circle for newer peers; newer peers see colored-circle for older peers because they never broadcast an avatar).

## 7. Out of scope for Phase 3

- **Curve editor for libmypaint input mappings.** Scalar-only param tuning ships; pressure/speed/stroke curves stay hardcoded per preset.
- **Sharing custom brush presets between artists.** Import/export of preset JSON files is a future polish item; v1 keeps presets strictly local.
- **Portal-published avatars** (e.g., for subscriber-facing artist branding). Avatars are collab-local only.
- **Animated avatars / video avatars / per-tool avatars.** One static image per artist, full stop.
- **Per-collab-session avatar override** (e.g. "use a different avatar in this session only"). Maybe useful later; out for Phase 3.
- **A brush preset marketplace.** Way too much. If sharing gets demand, plain-file import/export is sufficient.

## 8. Milestones

(Tentative; refine as workstreams firm up.)

| | | Description | Details |
|---|---|---|---|
| A1.M1 | | libmypaint param dump | Pull libmypaint's `brushsettings.json` (or equivalent) into a compile-time generated list of param IDs + ranges + group assignments; produce `MyPaintBrushParams.hpp` |
| A1.M2 | | BrushPreset struct expansion | Refactor `BrushPreset::apply` to drive from a full-param struct rather than 6-8 hardcoded calls; round-trip existing curated presets through the new struct + verify they render identically. Add optional `iconPath` field. |
| A1.M3 | | UserBrushPresets module | Disk scan / load / save / delete for `<configPath>/brush_presets/`. Handles param JSON + icon PNG sidecar pair. |
| A1.M4 | | Customization drawer UI | New side drawer + toolbar icon + parameter widgets (no curve editor); live mutation of active brush |
| A1.M5 | | Save-preset flow | Modal + write-through; refresh A2 drawer on save |
| Shared.M1 | | SquareCanvasCaptureTool | Fork of ButtonSelectTool with square-locked selection; parameterized by target output size + on-commit callback. Replaces what was sketched as a brush-specific `BrushIconCaptureTool`. Single tool, two call sites (A1 brush icons + B avatar). |
| A1.M6 | | Brush-icon capture wiring | "Capture from canvas..." button in A1 drawer activates `SquareCanvasCaptureTool` with target size 64×64 + callback that binds the captured PNG to the in-progress preset and saves on next Save |
| A2.M1 | | Curated preset PNG icons | Source / design / commission per-curated-preset PNGs in `data/icons/brush_presets/`. One per existing curated brush (tech-pen, fine-ink, brush-pen, wet-ink, smooth-pencil, textured-pencil, etc.). Plus `_no_icon.png` fallback. |
| A2.M2 | | SavedPresetsDrawer UI | New side drawer + toolbar icon; grid-of-tiles layout; search filter; tile click = activate preset |
| A2.M3 | | Preset context menu | Right-click / long-press: Rename, Edit, Delete, Duplicate. Edit opens A1 drawer with this preset loaded. |
| A2.M4 | | Drag-reorder + move-between-categories | User presets only; persist new order in filesystem (rename or metadata file) |
| A.M-phone | | Phone-UI parity for both drawers | Bottom-sheet or modal variant for `PhoneDrawingProgramScreen` covering A1 + A2 |
| B.M0 | | Clay-popover-over-image spike | Read-only investigation: can Clay render an interactive child popover over an image element with click hit-testing on the children? Pass / fail determines whether B.M3's `[cap]` `[file]` `[Clear]` buttons can sit directly over the avatar tile or have to open a separate modal positioned nearby. Block B.M2 / B.M3 design specifics on the result. |
| B.M1 | | AvatarStore module | `<configPath>/avatar.png` (256×256 master) + `<configPath>/avatar_wire.png` (64×64 wire form); save / load / regenerate-wire-form helpers |
| B.M2 | | Top-toolbar avatar tile | Render the 256-master scaled down inline at far-right of the canvas-view top bar; hash-derived placeholder tile when no avatar set |
| B.M3 | | Capture-from-canvas avatar wiring | Click avatar tile → popover with [cap] (active) + [file] (disabled, "future phase") + [Clear]. [cap] activates `SquareCanvasCaptureTool` with target size 256×256 + callback into AvatarStore. |
| B.M4 | | ClientData avatar field + wire path | Add 64×64 wire-form field, serialize, broadcast on join, decode + cache on receiver |
| B.M5 | | Avatar render on remote cursors | Update `draw_other_player_cursors` to composite remote avatars, colored circle stays as fallback edge indicator |
| B.M-phone | | Phone-UI parity for avatar tile | Phone top bar is more constrained; avatar tile likely moves into the main-menu popover. Capture flow + render are unchanged. |
| Release | | Phase 3 release | Rebrand, installers, release notes |

`Shared.M1` is a prerequisite for **both** A1.M6 and B.M3 — landing it first unblocks both workstreams' capture flows. Apart from that, Workstream A (A1.* + A2.*) and Workstream B (B.*) run in parallel; they touch separate code paths.

## 9. Open product questions

Almost everything from the first draft round resolved into the Decisions log below. Remaining open items:

- **Hash-derived placeholder library.** Decision §10 #8 picks "hash-derived placeholder if there's an easy drop-in, else generic fallback PNG." Need to scout: is there a small permissively-licensed C++ identicon / blockie / pattern library that takes a string hash and produces a 64×64 RGBA buffer? If yes, use it; if no, ship `_no_icon.png` and the generic-fallback path. Scout during A2.M2.
- **Avatar dimensions.** Decision §10 sets 256×256 master + 64×64 wire — no smaller / larger variants. If subscribers later report cursor-avatar clutter at 64×64, the wire size can drop to 32×32 in a future phase without protocol break (it's a downscale, not a schema change).
- **What if the existing tool-settings preset row consolidation happens later?** Phase 3 keeps the inline row as the "native / legacy set." If/when we revisit and decide to drop or reduce-to-button, that becomes its own scoped change. Not part of this phase.

## 10. Decisions log

| # | Date | Decision | Rationale |
|---|---|---|---|
| 0 | 2026-05-15 | **Color params excluded from brush presets.** All libmypaint `color_*` (base HSV) and `change_color_*` (per-dab modulation) params are filtered out of the customization drawer and never serialized to preset JSON. | A brush preset captures *feel*, not color. The artist's color picker is the sole authority on stroke color; switching brush should never change the active color. Per-dab color modulation as a brush trait (e.g. intentional hue-shift for grungy effects) is reserved for a separate future decision rather than implicitly inheriting via the catch-all "all params" rule. |
| 1 | 2026-05-15 | **Both drawers (A1 + A2) on the right side.** | Consistency with the layer-manager drawer from Phase 2; the canvas-left side stays clear. |
| 2 | 2026-05-15 | **Single-drawer rule.** Only one right-side drawer open at a time (A1, A2, or layer manager). Toggling open one auto-closes any other. | Avoids canvas obstruction and visual competition on narrower windows. Desktop + phone behave the same way. |
| 3 | 2026-05-15 | **Duplicate-to-edit yes; A1 Save-on-curated triggers Save-as-new.** A2's per-preset context menu (A2.M3) includes Duplicate; A1's Save button on a curated-preset-active brush triggers the Save-as-new modal rather than failing or overwriting. | Friendly entry point for "I want this one but slightly tweaked"; no copy-on-write complexity since curated presets stay read-only and the duplicate is a fresh user-preset file. |
| 4 | 2026-05-15 | **Existing inline brush-picker row stays as legacy / native set.** `render_brush_picker_row` in `gui_toolbox` / `gui_phone_toolbox` is unchanged for Phase 3. A2 surfaces the same curated presets in a better surface; both coexist. Consolidation deferred. | Avoids scope creep into the existing tool-popup wiring. The inline row is small, working, and not blocking the artist; revisiting later is cheap. |
| 5 | 2026-05-15 | **`data/icons/live-brush.svg`** (not `brush-compass.svg`) for the A1 drawer button. zynx is authoring; ship a placeholder copied from any existing icon under that filename until the real SVG lands. | Author choice. Filename change is one rename in `src/Toolbar.cpp` + `PhoneDrawingProgramScreen.cpp` when the real asset arrives. |
| 6 | 2026-05-15 | **`data/icons/brush-library.svg`** — basic library glyph for the A2 drawer button. | "A basic library icon will work for now" — can be sourced from an icon set or composed from existing glyphs. |
| 7 | 2026-05-15 | **Curated preset icons are PNG, not SVG.** Sourced into `data/icons/brush_presets/<name>.png`. | Matches the artist-captured-PNG medium for user presets so the A2 grid renders both kinds with a single image path; no special-case for SVG-vs-PNG in the renderer. |
| 8 | 2026-05-15 | **Missing-icon fallback: hash-derived placeholder if an easy drop-in library exists, else generic `_no_icon.png` shipped in the repo.** Same path used for user presets that never had an icon captured AND for cases where the PNG sidecar is missing on disk. Missing icon never fails-load the preset. | Hash-derived gives glanceable per-preset visual identity without manual art; generic fallback is the cheap safety net. Scout the library during A2.M2. |
| 9 | 2026-05-15 | **Subscriber-visibility rule.** When a remote `ClientData` is flagged `isViewer == true` (P0-D5), do NOT render its cursor or avatar on the host's view. COLLAB peers render normally. Implies reader-mode users (who are viewers from the host's perspective) are also hidden. | A working artist hosting a SUBSCRIPTION canvas shouldn't see subscriber cursors+avatars cluttering their canvas — subscribers are passive viewers, not collaborators, and visible cursors+avatars per-subscriber would interfere with painting once a canvas has more than a couple of subscribers. |
| 10 | 2026-05-15 | **No first-launch avatar prompt.** Silent default (no avatar until the artist explicitly captures one). Top-bar tile is always visible (hash-derived placeholder when empty), so discoverability comes from the always-present surface, not a one-time onboarding modal. | Consistent with the crypto-averse-by-default principle (`feedback_crypto_averse_users` memory): don't put identity-setup affordances in the artist's face on first launch; let them find these when they're ready. |
| 11 | 2026-05-15 | **Clay-spike-first for the avatar-tile popover.** Added as `B.M0` — a small read-only investigation that pass/fails the "image element with interactive overlay buttons" pattern before B.M2 / B.M3 design specifics are committed. If it fails, fallback to a click-opens-modal pattern. | The popover-over-image pattern is core to the avatar surface UX; verifying Clay supports it before designing more details around it avoids a redesign cost if the framework doesn't. |
| 12 | 2026-05-15 | **Phone avatar tile lives in the main-menu popover** (not inline in the phone top bar). | Phone top bar is already crowded with menu / tabs / layer dropdown / reader toggle; an inline avatar tile would crowd it further. Capture flow + render path are unchanged from desktop. |
| 13 | 2026-05-15 | **Capture-rect max size: 2048 source pixels per side. No soft-warn for low-resolution captures — just capture.** | Caps the rect at a sane upper bound (well above what 64×64 or 256×256 downscale needs) and prevents the pathological case of an artist trying to capture the whole canvas at extreme zoom-out. If a capture looks weird at low source resolution, the artist re-captures at higher zoom; no extra UI for that. |
