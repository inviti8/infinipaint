#pragma once
#include <optional>
#include <string>

// DISTRIBUTION-PHASE1.md §4 — process spawn/IPC test harness.
//
// Isolated test surface for the cross-platform process primitives we'll
// need to wire side-instance auto-hosting (per DP1-B redesign). Lives
// behind `--test-*` argv flags handled before SDL/MainProgram init, so
// these tests run with zero canvas, zero NetLibrary, zero World — purely
// the spawn / kill / stdin-stop / parent-death / lock-handoff plumbing.
//
// Flags handled by dispatch():
//
//   Parent-side (run the test, exit 0 on pass):
//     --test-spawn-roundtrip
//     --test-spawn-kill
//     --test-spawn-stdin-stop
//     --test-spawn-multi <N>
//     --test-spawn-orphan-detect <result-file>
//     --verify-orphan-detect <child-pid> <result-file> [<timeout-sec>]
//     --test-lock-handoff <canvas-path>
//     --test-host-only-roundtrip <canvas-path> [<idle-seconds>]
//                              spawn the real --host-only side-instance,
//                              wait for READY, idle N seconds, STOP,
//                              verify clean exit + lock released
//     --test-side-instances <canvas-path> [<idle-seconds>]
//                              exercise the SideInstances manager: spawn
//                              one side-instance, verify lock + is_alive
//                              + is_managing, idle, verify dtor stops it
//                              and releases the lock
//     --test-scan-and-spawn <saves-dir> [<idle-seconds>]
//                              launch-time flow: scan saves-dir for
//                              .publish markers, spawn a side-instance
//                              for each unlocked marker, verify all are
//                              alive + locked, idle, verify dtor stops
//                              all and releases all locks
//     --test-all-spawn         (runs the deterministic non-orphan suite)
//
//   Child-side (run by the harness, not invoked manually):
//     --test-child-exit-zero
//     --test-child-loop-forever
//     --test-child-stdin-loop
//     --test-child-orphan-detect <parent-pid> <result-file>
//     --test-child-lock-claim <canvas-path>

namespace ProcessTests {

// Resolve the absolute path of the running executable on the current
// platform. Used by parent-side tests to spawn another instance of
// themselves. Falls back to argv0 if the platform call fails.
std::string resolve_self_exe(const char* argv0);

// Called once from main.cpp before dispatch, so child-spawning tests
// know which binary to launch.
void set_self_exe_path(const std::string& path);
const std::string& self_exe_path();

// If argv matches a `--test-*` or `--test-child-*` flag, run the test
// and return its exit code. Returns nullopt if no flag matched, in
// which case main proceeds with normal app startup.
std::optional<int> dispatch(int argc, char** argv);

}  // namespace ProcessTests
