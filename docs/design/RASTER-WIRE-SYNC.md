# Raster brush wire sync — Phase 1 closeout

## Status

**Phase 1 blocker.** v0.12.0-rc1 / rc2 are out, but the clean v0.12.0
release should not happen until this lands. Workaround for existing
RC users: convert raster strokes to vector via the StrokeVectorize
tool, which produces a `BrushStrokeCanvasComponent` that *does* sync.

## Problem

Cross-instance Inkternity collaboration works for every
`CanvasComponent` subclass *except* `MyPaintLayerCanvasComponent` —
the per-stroke raster layer that owns a `LibMyPaintSkiaSurface` and
holds the actual pixels for every libmypaint brush stroke (Ink set,
Pencil set, eraser dabs into existing raster layers).

Symptom: local artist paints with a libmypaint brush; the stroke
shows correctly on their canvas, saves to disk correctly, but never
appears on remote collaborators' canvases.

## Root cause

`src/CanvasComponents/MyPaintLayerCanvasComponent.cpp` has four
NetObj-side stubs:

| Method | Disk equivalent | Status |
|---|---|---|
| `save(cereal::PortableBinaryOutputArchive&)` | `save_file()` writes tile data via `surface_->save_tiles_to_archive(a)` plus the recording fields | **Empty stub** |
| `load(cereal::PortableBinaryInputArchive&)` | `load_file()` calls `load_tiles_from_archive(a)` for v0.7+ files | **Empty stub** |
| `get_data_copy()` | — | Returns fresh empty surface (loses tiles) |
| `set_data_from(const CanvasComponent&)` | — | Empty body (loses mutations) |

Lineage: `PHASE1.md` §M3 shipped libmypaint as a session-only
feature; tile-archive serialization was a deferred follow-up that
*did* land (v0.7+ file format) for disk persistence. The wire path
was the same kind of follow-up but was never recorded in the
"M3 follow-ups deferred" list and so silently fell off.

Every other component subclass has a working `save()` /
`load()` pair (BrushStroke, Ellipse, Image, Rectangle, TextBox,
Waypoint). The gap is exactly one component.

## Scope

In: `src/CanvasComponents/MyPaintLayerCanvasComponent.cpp` —
implementations of `save()`, `load()`, `get_data_copy()`,
`set_data_from()`. Plus end-to-end test on two Inkternity instances
that drawing with each libmypaint preset replicates correctly. Plus
the same verification for the eraser interaction
(`erase_along_segment`) which mutates the surface in-place and so
inherits the same sync path.

Out: the entire StrokeVectorize side (its output is a vector
component that already works). The libmypaint stroke recording path
on disk is not touched. The brush preset registry, the tool
hierarchy, the layer manager — none affected.

## Decisions

### D1. Wire representation

**Decision: send tile bitmap data (Option A — "do what the disk
format does").** Mirror the disk path by calling
`surface_->save_tiles_to_archive(a)` /
`load_tiles_from_archive(a)` from `save()` / `load()`. Same encoded
form, same on-the-wire bytes as what hits disk.

Rejected alternative: send the recorded stroke path
(`recordedSamples_` + brush preset + color + base radius) and have
the remote re-run it through libmypaint. Smaller payload but adds
three new failure modes — preset-ID sync between peers, libmypaint
determinism across CPU architectures, fallback handling for
eraser-touched strokes where `invalidate_recording()` has dropped
the path. Worth doing later if bandwidth becomes a real complaint;
not justified for the closeout fix.

### D2. Mid-stroke updates

**Decision: full final-stroke sync only.** The wire-side
serialization fires when the component is registered with the
NetObjOrderedList; subscribers see the stroke appear on pen-up.
Mid-stroke progressive rendering on remote is nice-to-have, not
shipping-critical, and requires the NetObj framework to support
incremental mutation broadcasts on the same object identity (or
us to pre-allocate the component at pen-down and use set_data_from
to push tile-diff frames during the stroke — a separate effort).

Outcome: a remote collaborator sees a libmypaint stroke "pop into
existence" 100–500 ms after the local artist lifts their pen,
proportional to stroke size + network latency. Same UX as the
existing vector-brush stroke replication.

### D3. Eraser

**Decision: covered automatically by D1/D2.** Eraser modifies the
existing component's surface via `erase_along_segment`. As long as
the NetObj framework detects the mutation (or the eraser tool
explicitly marks the component dirty via `set_data_from` semantics
on the owner's local copy), the next sync pass picks up the eroded
state via `save()` and re-broadcasts.

**Verification task in implementation:** confirm that NetObjOrderedList
notifies on element mutation (not just insert/erase). If not,
eraser sync needs an explicit "mark this element dirty" trigger
from the eraser tool. Both paths are within scope for this fix.

### D4. Bandwidth

**Decision: accept it.** Typical libmypaint stroke touches a few
tiles; a tile is 64×64 RGBA fixed-point ≈ 16 KB raw, less after the
`save_tiles_to_archive` packing. A medium stroke is likely 50–200 KB
on the wire, large strokes might hit 1 MB.

NetLibrary already fragments at `FRAGMENT_MESSAGE_STRIDE = 512`
bytes per fragment and tolerates `MAX_BUFFERED_DATA_PER_CHANNEL =
64 KB` per channel. Worst case is a 1-second pause for the
subscriber on a slow link before a fat stroke shows up. Acceptable
for Phase 1.

Mitigation if/when this becomes a real complaint: D1's Option B
(replay from recording) drops typical strokes to a few KB. Document
as a Phase 2 optimization in `PHASE2.md`.

### D5. Initial-state sync for new subscribers

**Decision: verify, then patch if needed.** When a new subscriber
connects, they receive a snapshot of the canvas. The mechanism may
be either (a) `save_file()` dumped to a resource-channel transfer,
which already serializes tiles correctly, or (b) iterating every
component's `save()`, which currently loses tiles.

**Verification task in implementation:** trace the NetObj initial-state
broadcast path. If (a), no additional work — the disk format
covers it. If (b), the D1 fix automatically covers initial state
as well, since `save()` will now serialize tiles.

## Implementation outline

1. Read `LibMyPaintSkiaSurface::save_tiles_to_archive` /
   `load_tiles_from_archive` to confirm the format is wire-safe
   (no version-dependent fields, no host-architecture assumptions).
2. Implement `MyPaintLayerCanvasComponent::save()` /  `load()` as the
   verbatim wire equivalents of `save_file()` / `load_file()`.
3. Implement `get_data_copy()` to clone the surface contents (new
   helper on `LibMyPaintSkiaSurface` if one doesn't exist —
   tile-by-tile copy).
4. Implement `set_data_from(const CanvasComponent& other)` to
   replace this component's surface state with `other`'s.
5. Verify NetObjOrderedList mutation notification (D3 verification).
   If the eraser doesn't sync, patch the tool to mark the touched
   component dirty.
6. Trace initial-state sync path (D5 verification). Patch if
   needed.
7. End-to-end test: two Inkternity instances on different machines,
   one Hosting in COLLAB or SUBSCRIPTION mode, the other joining.
   Verify each libmypaint preset draws on the remote. Verify the
   eraser propagates. Verify a new join mid-session sees all
   existing libmypaint strokes.

## Acceptance criteria

- Two Inkternity instances in collab; libmypaint stroke drawn on
  instance A appears on instance B within ~1s of pen-up.
- Same in the reverse direction (B → A).
- Eraser dabs from either instance propagate.
- New subscriber joining mid-canvas sees existing libmypaint
  strokes in the snapshot they receive.
- Disk save / load is not regressed.
- No new wire-format version field is needed (the wire payload is
  the same as the disk payload for libmypaint tiles).

## Out of scope

- Bandwidth optimization via recording-replay (D1 alternative).
- Mid-stroke progressive sync (D2 alternative).
- Pixel-streaming libmypaint output to subscribers in real time.
- Compression of tile data beyond what `save_tiles_to_archive`
  already does.
- Cross-version compat for the wire payload — this is a new
  capability, both peers must run the same wire format. The on-disk
  version gate (`if (version >= 0.7.0)`) protects file loads, but
  the wire is between peers running the same build.

## Versioning

This is bundled into v0.12.0 as the closeout item before the clean
v0.12.0 release tag. The fix lands on `main`, gets rolled into
`v0.12.0-rc3`, and after smoke-testing becomes the non-RC
`v0.12.0`.

## Estimated effort

- Reading + implementation: 1–2 hours.
- Two-machine collab smoke test: 30 min (requires the user's
  second machine).
- Buffer for D3 / D5 verification surprises: another 1–2 hours.

Total: half a day, plus the two-machine test cycle.
