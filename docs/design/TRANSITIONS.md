# Inkternity — Transition Points Design Doc

> **Audience:** the agent (and any human contributor) working inside the Inkternity repo.
>
> **Goal of this doc:** define the transition-point feature — a Waypoint variant that acts as an auto-advanced intermediary on the reader's path — including data model, UI, reader behavior, edge cases, file format, and a milestone breakdown.

## 1. Product summary

A **transition point** is a Waypoint variant that the reader does not stop at by choice. The camera arrives, optionally pauses for a configured `stopTime`, then auto-advances along the single outgoing edge to the next point (which may itself be another transition point, forming a chain).

Use case: multi-beat camera reveals inside what the reader perceives as a single panel — e.g. zoom in on a face, hold for 1s, zoom out to the room. Today this would require either (a) a single waypoint that does both at once (impossible — one camera state per waypoint), or (b) two real waypoints with a forced "click" in between (breaks the reading flow). Transition points let the artist author camera choreography between user-controlled stops.

The artist composes a directed path:

```
Waypoint A  →  Transition P1 (stop 1.0s)  →  Transition P2 (stop 0.5s)  →  Waypoint B
```

The reader sees two stops (A and B) and a single auto-played camera sequence between them.

## 2. Inheritance from Phase 2

Phase 2 leaves all of the necessary infrastructure in place:

- `Waypoint` data model with NetObj sync, save/load, scale-up, skin storage
- Per-waypoint transition speed (`transitionSpeedMultiplier`) and easing (`transitionEasing`) — applied to the camera move *into* the waypoint
- `WaypointGraph` with edges (`NetObjOrderedList<Edge>`), node list, selection
- `WaypointMarker` canvas component for in-canvas rendering
- `WaypointTool` for placement + per-waypoint settings panel
- `EditTool` selection / drag / delete on markers
- Reader-mode traversal (`navigate_to`, `forward`, `back`, `outgoing_choices`) with smooth-camera and history stack
- `BezierEasing` + `DrawCamera::smooth_move_to` already accept per-call speed multiplier and easing override

Transition points are an additive layer on top of this. No existing Phase-1/Phase-2 functionality changes shape — every new behavior is gated by `Waypoint::is_transition()`.

## 3. Data model

Add two fields to `Waypoint`:

```cpp
bool   isTransition = false;     // marks this waypoint as a transition point
float  stopTime     = 0.0f;      // seconds the camera pauses after arrival
                                 // before auto-advancing. Ignored when
                                 // isTransition == false. Range: 0.0 .. 5.0.
```

Both are absent on every Phase-1/Phase-2 file by default — `false` and `0.0` reproduce existing waypoint behavior exactly.

Getters/setters mirror the speed/easing pattern from Phase 2:

```cpp
static constexpr float TRANSITION_STOP_TIME_MIN     = 0.0f;
static constexpr float TRANSITION_STOP_TIME_MAX     = 5.0f;
static constexpr float TRANSITION_STOP_TIME_DEFAULT = 0.0f;

bool  is_transition()         const { return isTransition; }
void  set_is_transition(bool v)     { isTransition = v; }
bool& mutable_is_transition()       { return isTransition; }

float get_stop_time()         const { return stopTime; }
void  set_stop_time(float t)        { stopTime = std::clamp(t, ..., ...); }
float& mutable_stop_time()          { return stopTime; }
```

**Why a flag, not a subclass:** a separate `TransitionPoint` class would force parallel paths in serialization, networking (a second NetObj class registration), the graph (which accepts heterogeneous nodes how?), the marker (which canvas-component type?), and the tree-view (two list-item kinds?). The flag adds two fields and changes behavior in two places (reader auto-advance + edge-add enforcement). All other code reads it as a normal Waypoint.

## 4. Editor surface

### Creating a transition point

The `WaypointTool` settings panel (already shown when a waypoint is selected) gains:

- **Checkbox: "Transition point"** — toggles `isTransition`.
- **Slider: "Stop time (s)"** — editable only when the checkbox is checked. Range 0.0–5.0, step 0.1. Hidden when unchecked.

The user can convert any existing waypoint to a transition point and back. Toggling has no destructive effect — speed, easing, edges, skin all persist. Toggling *off* a transition point with multiple incoming edges is fine; toggling *on* with multiple outgoing edges triggers the enforcement rule below.

No new tool. Transition points are placed exactly like waypoints; the `isTransition` flag is set after-the-fact.

### Marker visual

`WaypointMarker::draw` switches geometry based on `is_transition()`:

- **Waypoint (existing):** filled circle.
- **Transition point (new):** filled diamond (square rotated 45°), same fill color, smaller — ~70% the radius of a waypoint dot. Visually subordinate to real waypoints since they aren't reader-stops.

The selected/hover outline treatment stays the same (just follows the new outline path).

Rationale for diamond: visually distinct enough to read at a glance, matches the "stepping-stone" mental model (waypoints are anchors, transitions are step-stones in between), and doesn't collide with any other marker shape in the codebase.

### Tree-view

`DrawingProgramTreeView` waypoint rows get a small badge or icon prefix when the row's waypoint `is_transition()` — likely a tiny diamond glyph next to the label. No structural change to the tree — transition points sort and behave like any other waypoint there.

### Edge-creation enforcement

When the user creates a new outgoing edge from a transition point (via the existing connect-drag UX), `WaypointGraph::add_edge` (or its caller) checks: does this `from` node already have an outgoing edge AND is it a transition point? If yes:

1. Delete the existing outgoing edge first (single-call, undo-grouped with the new edge).
2. Then add the new edge.

This keeps the invariant *"a transition point has at most one outgoing edge"* enforceable in the data model itself, which means the reader's auto-advance has a single unambiguous target without any "pick the first" silent behavior.

If this enforcement turns out to be infeasible at the edge-creation site (e.g. multi-user race — a remote peer adds an edge concurrently), the fallback contract is: **`forward()` always picks the first outgoing edge silently.** No UI surfaces a chooser at a transition point under any circumstances. The graph might transiently hold 2+ outgoing edges from a transition point during a sync window; that's acceptable as long as reader behavior stays deterministic.

Outgoing edges *into* a transition point are unconstrained — multiple waypoints/transitions can point to the same transition point.

## 5. Reader-mode behavior

### Auto-advance state machine

Currently `ReaderMode` has no per-frame `update()` — all state changes are event-driven (key/click → `navigate_to` → camera smooth-move begins → next user input). Auto-advance needs a tick.

Add `void ReaderMode::update(float deltaTime)` called from the same place `DrawCamera::update_main` is called per frame.

State (added to `ReaderMode`):

```cpp
enum class TransitionPhase : uint8_t {
    IDLE,            // not at a transition point, or done with one
    CAMERA_MOVING,   // camera smooth-move is in flight to the transition point
    PAUSING,         // camera arrived, pausing for stopTime seconds
};
TransitionPhase   phase = TransitionPhase::IDLE;
float             pauseTimeRemaining = 0.0f;
NetObjID          autoAdvanceTarget;   // edge target snapshotted at arrival
```

Flow:

1. **`navigate_to(id)` lands on a transition point** → set `phase = CAMERA_MOVING`. Snapshot the first outgoing edge's target into `autoAdvanceTarget`. (Or set IDLE + flag for "this is a dead-end transition" if no outgoing edges — see edge cases.)
2. **Per-frame `update()`** while `phase == CAMERA_MOVING`: check if the camera's smooth-move has completed. When it has, transition to `PAUSING` with `pauseTimeRemaining = current.stopTime`.
3. **Per-frame `update()`** while `phase == PAUSING`: decrement `pauseTimeRemaining` by `deltaTime`. When it hits 0, call internal `auto_advance_to(autoAdvanceTarget)` and set `phase` based on the new current node (CAMERA_MOVING again if next is also a transition, IDLE if it's a real waypoint).
4. **`auto_advance_to`** is `navigate_to` minus the history push (see history rule below) and minus the user-input gating.
5. **Phase resets to IDLE** on: `set_active(false)`, user `back()`, user `forward()` keypress, or any user `navigate_to` click on a branch button. Any user input cancels the pending auto-advance.

### Camera-arrival detection

Avoid polling `cam.is_smooth_move_active()` directly — that pattern bit us during the reader-mode polish work. Instead, snapshot the smooth-move's `targetDuration` at navigation time and tick our own timer:

```cpp
float cameraTimeRemaining = 0.0f;  // alongside pauseTimeRemaining

// On entering CAMERA_MOVING phase:
cameraTimeRemaining = configured_duration_for_target_waypoint();

// Per frame in CAMERA_MOVING:
cameraTimeRemaining -= deltaTime;
if (cameraTimeRemaining <= 0.0f) phase = PAUSING;
```

The duration used here is the same value `DrawCamera::smooth_move_to` snapshots into `SmoothMove::targetDuration` (`jumpTransitionTime / speedMultiplier`). Compute it once at the navigation site and stash it locally. This decouples our auto-advance timing from any future change to how `DrawCamera` reports its in-flight state.

### History rule

Transition points **never push onto `history`**. When the reader presses Back from waypoint B in the chain `A → P1 → P2 → B`, history pops back to A directly — they should never see the editor's intermediate camera positions as user-navigable stops. Implementation: in `navigate_to`, push the current id onto history *only if `current.is_transition() == false`*; the auto-advance path also never pushes.

This means the next forward() from A may auto-replay the P1→P2→B chain — which is the correct behavior: a transition path is part of the *edge*, not the destination. The reader sees the same transition the second time through.

### User input during auto-play

Any of the following while `phase != IDLE` cancels the pending auto-advance and acts on the user input:

- Back keypress / button click → `back()` (navigates to history top, regardless of where the camera currently is)
- Forward keypress → if there's an outgoing edge from current, take it; else no-op
- Branch-button click → `navigate_to(target)`
- Tap-anywhere-to-skip on phone (optional polish; defer to a follow-up)

The cancel rule is: setting `phase = IDLE` is sufficient — the next user-driven navigation goes through the normal path and any in-flight camera move continues toward its target (the camera is decoupled from the auto-advance state machine).

### Branch overlay

`render_reader_branch_overlay` already early-returns when the current node has no choices and no history. For transition points specifically, suppress branch buttons even if the current node has outgoing edges:

```cpp
if (world.readerMode.current_is_transition()) {
    // Render only the back button (if has_history). No outgoing-choice
    // buttons — the reader doesn't pick from a transition point.
    ...
}
```

This protects against the (post-enforcement) edge case where a transition point has 2+ outgoing edges — the UI silently shows nothing while the auto-advance silently picks the first. The data model invariant should prevent this in practice, but the UI defends against the data being temporarily out-of-invariant.

## 6. Edge cases

| Case | Behavior |
|---|---|
| User adds an outgoing edge to a transition point that already has one | Old edge deleted first, new edge replaces it. Both operations group into one undo entry. |
| Edge enforcement infeasible (e.g. multi-user race) | `forward()` / auto-advance always picks the first outgoing edge silently; UI never offers a choice from a transition point |
| Transition point with 0 outgoing edges | Treated as the end of the chain. `stopTime` is **ignored**. Reader sees it as a dead-end waypoint (pre-existing dead-end behavior fires: M7-d "the end" affordance). |
| Transition point with 0 outgoing edges, with history | Back button still renders. Reader can step backward. |
| User toggles reader mode off mid-chain | `set_active(false)` resets `phase = IDLE` and clears `pauseTimeRemaining`. No pending callbacks to clean up. |
| User clicks Back during a `PAUSING` phase | Auto-advance cancelled; history pops; camera smooth-moves to the popped target. |
| User clicks a branch button from a real waypoint that auto-advanced TO it | Branch is taken normally — by the time branch buttons are visible, `phase` is already IDLE (we're at the real waypoint). |
| Cycle: A → P1 → P2 → P1 (loop) | The reader spins forever. **Mitigation:** auto-advance walk in `update()` increments a per-navigation `chainHopCount`; if it exceeds `MAX_TRANSITION_CHAIN = 32`, log a warning and stop at the current node (treats it as a dead-end). The artist sees a one-time visual debug marker on the cyclic node in editor mode. |
| Transition point's `transitionSpeedMultiplier` and `transitionEasing` | Apply to the camera move *into* this transition point, exactly as for normal waypoints. The `stopTime` pause is independent and starts after the move completes. |
| Transition point as the reader-mode entry point | Allowed. Auto-advance starts immediately on `set_active(true)`. The first node the reader actually *stops* at is the first non-transition node downstream. |
| Eraser hits a transition-point marker | Same protection as waypoints — ERASER skips `CanvasComponentType::WAYPOINT` regardless of the `isTransition` flag. (Already correct, no change needed.) |

## 7. File format

Bump `INFPNT000011` → `INFPNT000012` (Phase-2's last bump was 0.9 → 0.10 for stroke logs; this is 0.10 → 0.11 for transition fields). Confirm the actual current header at implementation time and bump from there.

`Waypoint::save_file` appends, after the existing transition-data block:

```cpp
a(isTransition);
a(stopTime);
```

`Waypoint::load_transition_data_from_archive` (or a new sibling) gates on file version `>= 0.11.0` and reads the two fields. Older files default to `isTransition=false, stopTime=0.0` — produces identical pre-existing behavior.

## 8. Risks

- **Auto-advance state machine.** Same risk class as the reader-mode polish that didn't ship: any logic that has to coordinate with the camera's in-flight state can get tangled. Mitigation: snapshot duration at navigation time, run our own timer, never poll `DrawCamera`. Test with a ≥3-link chain, with mid-chain back, with toggle-off mid-chain, with a 0s stopTime (instant pass-through), and with a cycle.
- **Edge enforcement vs. multi-user.** Single-user enforcement is straightforward. In a future multi-user world, two peers can each add an outgoing edge to the same transition point in the same sync window. The "first outgoing wins" fallback contract makes this non-fatal, but the UI may need a "this transition point has multiple outgoing edges" warning badge in the tree-view. **Defer until multi-user actually re-lights** — Inkternity is single-user today.
- **Cycle detection upper bound.** `MAX_TRANSITION_CHAIN = 32` is arbitrary. Anything past ~8 hops is almost certainly an authoring mistake. The 32 ceiling is loose enough not to false-positive on legitimately long camera choreographies, tight enough to bail before the reader notices a freeze.
- **Toggling a normal waypoint on/off as transition** with multiple outgoing edges → the user toggles a branch-point ON as a transition. Now there's a constraint violation. Apply the enforcement at toggle time: if the user converts a multi-out waypoint to a transition, surface a confirm dialog ("This will delete N outgoing edges, keeping the first only. Continue?") rather than silently destroying edges.
- **Camera-arrival false trigger.** Our local camera timer counts down by `deltaTime`, same as `DrawCamera`'s. If the two diverge (e.g. someone changes the camera's animation curve), we could think we've arrived before the camera actually has. Acceptable: the worst case is the pause starts a frame or two early, then auto-advance kicks the next move while the camera is still finishing the prior one. `smooth_move_to` handles re-entering smooth-move from a non-stationary position via the `start` snapshot, so no visual artifact.

## 9. Subtasks

| | Deliverable |
|---|---|
| T1 | `Waypoint` data model: `isTransition` + `stopTime` fields, getters/setters, save/load, format bump 0.10 → 0.11 |
| T2 | `WaypointTool` settings UI: checkbox + stop-time slider; gating (slider hidden when checkbox off) |
| T3 | `WaypointMarker` visual variant: diamond geometry path branch on `is_transition()` |
| T4 | Tree-view badge for transition rows |
| T5 | `WaypointGraph::add_edge` enforcement: deletes old outgoing edge from transition source, inserts new (single undo) |
| T6 | Toggle-on guard: confirm-dialog when converting a multi-outgoing waypoint to a transition (deletes N-1 edges) |
| T7 | `ReaderMode::update(float)` + `TransitionPhase` state machine; per-frame call wiring |
| T8 | History-skip rule: `navigate_to` and auto-advance never push transition ids onto history |
| T9 | Branch-overlay suppression at transition points |
| T10 | Cycle detection: chain-hop counter with `MAX_TRANSITION_CHAIN` cap + warning log |
| T11 | Dead-end behavior at transition-with-no-outgoing: ignore `stopTime`, fall through to existing dead-end "the end" path |
| T12 | Manual test pass: chain of 3 transitions with varied stopTimes; mid-chain back; toggle reader off mid-pause; cycle (verify cap); 0s stopTime; transition as entry point |

T1–T6 are local, low-risk additive changes. T7 is the load-bearing piece. T8–T11 are small but each one needs a clear test in T12.

## 10. Out of scope

- **Per-edge transition controls.** A "this edge has speed X / easing Y" overlay on the edge, instead of (or in addition to) per-waypoint. Adjacent feature, deferable.
- **Skip-pause on user input.** Tapping during a `PAUSING` phase to immediately advance. Easy to add later (one line in `update()`); explicit user request when it surfaces.
- **Multi-user concurrent edge adds at a transition point.** Single-user works correctly; multi-user gets the "first edge wins" fallback. Real fix waits for multi-user revival.
- **Custom easing curve editor at transition points.** Inherits from Phase 2's deferred custom-curve work — same dropdown of presets applies here, same Phase-3-territory note.
- **Transition-point thumbnail/skin.** A transition point doesn't surface in the branch UI, so a skin would never be displayed. Storing one would just bloat the file. Save/load skips the skin payload entirely when `isTransition == true` — or, simpler, allows it but never reads it back at render time. (Pick the "allow but ignore" path to keep save/load symmetric with regular waypoints.)
- **Visual indicator on edges that traverse transition chains.** "This edge auto-plays a 3-step camera move" hint in the tree-view or canvas. Nice-to-have, defer.
