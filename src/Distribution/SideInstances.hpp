#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// DISTRIBUTION-PHASE1.md §4 — main-process side-instance management.
//
// Owns the map of spawned `--host-only` side-instances and their
// lifecycle. The main Inkternity process spawns one side-instance per
// published canvas at launch (and on Publish-toggle in the Toolbar);
// each side-instance is a child OS process running the headless host
// for one canvas (`src/Distribution/HostOnly.cpp`).
//
// Lifecycle model:
//   - `spawn(path)`: SDL_CreateProcess of `<self_exe> --host-only PATH
//     --parent-pid <our-pid>`. Returns immediately (fire-and-forget);
//     READY synchronisation is the caller's concern when needed (e.g.
//     foreground takeover).
//   - `stop(path)`: write "STOP\n" to the side-instance's stdin, wait
//     graceful exit up to STOP_TIMEOUT, fall back to SDL_KillProcess
//     on timeout. Removes from map regardless.
//   - dtor: stop() every managed side-instance. Blocking call; can take
//     several seconds × N if processes are slow to exit.
//
// Lock coordination: SideInstances does NOT lock canvas files itself.
// Locking is the side-instance's responsibility (via
// PublishedCanvases::try_acquire_lock inside HostOnly::run_host). Main
// process callers should check `PublishedCanvases::is_locked_by_anyone`
// *before* calling spawn() — if a lock is already held (by a
// side-instance from this Inkternity, by a side-instance from another
// Inkternity, or by another main process), spawning is pointless.
//
// Threading: all methods are called from the main UI thread. No
// internal locking.

class SideInstances {
public:
    // selfExePath: absolute path to the running inkternity binary.
    // Used as argv[0] for spawn calls. If empty, spawn() will fail.
    explicit SideInstances(const std::string& selfExePath);
    ~SideInstances();

    SideInstances(const SideInstances&) = delete;
    SideInstances& operator=(const SideInstances&) = delete;

    // Spawn a `--host-only` side-instance for the given canvas. Returns
    // true on successful SDL_CreateProcess + record-in-map. No-op + returns
    // true if we're already managing this canvas. Returns false on
    // SDL_CreateProcess failure or if selfExePath is empty.
    //
    // The side-instance is fire-and-forgotten — this call does NOT wait
    // for it to start hosting. start_hosting completes inside the
    // side-instance in ~1-2s; subscribers can connect via the signaling
    // server as soon as that finishes.
    bool spawn(const std::filesystem::path& canvasPath);

    // Walk the saves directory for canvases with publish markers and
    // spawn a side-instance for each one whose lock isn't already held.
    // Useful at app launch. Returns the number of side-instances spawned
    // (excluding ones that were already locked / already managed).
    size_t scan_and_spawn(const std::filesystem::path& savesDir);

    // Signal a managed side-instance to flush + exit. Blocks up to
    // STOP_TIMEOUT seconds for graceful exit; on timeout SDL_KillProcess
    // forces it. Removes from map regardless. Returns true if we were
    // managing it (whether or not the exit was graceful); false if the
    // canvas wasn't in our map.
    bool stop(const std::filesystem::path& canvasPath);

    // Stop every side-instance we own. Called by dtor. Idempotent.
    void stop_all();

    // True iff this instance is currently in our map (i.e. we spawned
    // and haven't stopped it).
    bool is_managing(const std::filesystem::path& canvasPath) const;

    // Snapshot of currently-managed canvas paths. Order is not stable
    // across calls.
    std::vector<std::filesystem::path> managed() const;

    // Polls whether the underlying OS process for this canvas is still
    // alive. Returns false if not managed OR if the process has exited.
    // Cheap (single SDL_WaitProcess non-blocking call); fine to call per
    // frame for UI status if needed.
    bool is_alive(const std::filesystem::path& canvasPath) const;

    static constexpr int STOP_TIMEOUT_SECONDS = 3;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
