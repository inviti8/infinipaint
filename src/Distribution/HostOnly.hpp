#pragma once
#include <optional>

// DISTRIBUTION-PHASE1.md §4.3 — `--host-only` side-instance entry point.
//
// Spawned by the main Inkternity process (one side-instance per
// published canvas), this entry point runs a headless host: no SDL
// window, no Skia GPU context, no UI. Just a `World` + `NetLibrary`
// serving subscribers for the assigned canvas. Lifecycle is driven by
// the parent process via piped stdin ("STOP\n" → graceful exit) and
// optionally by parent-PID polling for orphan detection.
//
// Usage:
//   inkternity --host-only <canvas-path> [--parent-pid <pid>]
//
//   --host-only <canvas-path>   absolute path to the canvas file
//   --parent-pid <pid>          optional. If provided, side-instance
//                               self-terminates when the parent
//                               process is no longer alive. Skipped
//                               when invoked standalone (e.g. from
//                               the test harness).
//
// Exit codes:
//   0   clean shutdown (STOP received, or parent gone)
//   1   load/host failure (canvas missing, NetLibrary init failed, …)
//   2   lock acquisition failed — another process owns it already

namespace HostOnly {

// If argv matches the `--host-only` flag, run the side-instance and
// return its exit code. Returns nullopt if not the host-only flag, in
// which case main proceeds with normal app startup.
//
// Must be called *before* SDL_INIT_VIDEO / MainProgram window setup —
// the host-only path does its own minimal SDL init and constructs a
// MainProgram with `window.sdlWindow == nullptr`.
std::optional<int> dispatch(int argc, char** argv);

}  // namespace HostOnly
