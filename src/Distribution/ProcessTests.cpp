#include "ProcessTests.hpp"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_error.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../PublishedCanvases.hpp"
#include "SideInstances.hpp"
#include <Helpers/Logger.hpp>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <process.h>
#else
    #include <cerrno>
    #include <signal.h>
    #include <sys/types.h>
    #include <unistd.h>
    #ifdef __APPLE__
        #include <mach-o/dyld.h>
    #endif
#endif

namespace ProcessTests {

namespace {

std::string g_self_exe;

// Logs go to stderr so the child's stdout stays a clean protocol channel
// (parents parse "READY <pid>", "ACQUIRED <pid>", "STOPPED", etc. there).
void log(const std::string& s) {
    std::cerr << "[ProcessTests] " << s << std::endl;
}

int my_pid() {
#ifdef _WIN32
    return static_cast<int>(_getpid());
#else
    return static_cast<int>(getpid());
#endif
}

// Returns true iff a process with this PID currently exists and has not
// exited. Cross-platform; mirrors the stale-PID check already used in
// PublishedCanvases.cpp but kept local to keep the harness self-contained.
bool process_alive(int pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                           FALSE, static_cast<DWORD>(pid));
    if (!h) return false;
    DWORD r = WaitForSingleObject(h, 0);
    CloseHandle(h);
    return r != WAIT_OBJECT_0;
#else
    if (kill(static_cast<pid_t>(pid), 0) == 0) return true;
    return errno != ESRCH;
#endif
}

void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ---- child-side test bodies ---------------------------------------------

int run_child_exit_zero() {
    log("child exit_zero: pid=" + std::to_string(my_pid()) + ", exiting 0");
    return 0;
}

int run_child_loop_forever() {
    log("child loop_forever: pid=" + std::to_string(my_pid()));
    for (;;) sleep_ms(100);
}

int run_child_stdin_loop() {
    log("child stdin_loop: pid=" + std::to_string(my_pid()));
    std::cout << "READY " << my_pid() << std::endl;
    std::cout.flush();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "STOP") {
            std::cout << "STOPPED" << std::endl;
            std::cout.flush();
            return 0;
        }
        log("child stdin_loop: ignored line: " + line);
    }
    log("child stdin_loop: stdin closed without STOP, exiting 0");
    return 0;
}

int run_child_orphan_detect(int parent_pid, const std::string& result_file) {
    log("child orphan_detect: ppid=" + std::to_string(parent_pid) +
        " result=" + result_file);
    const auto start = std::chrono::steady_clock::now();
    const int timeout_ms = 30000;
    const int poll_ms = 200;
    while (true) {
        if (!process_alive(parent_pid)) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            std::ofstream f(result_file);
            f << "DETECTED " << elapsed << "\n";
            f.close();
            log("child orphan_detect: parent dead after " +
                std::to_string(elapsed) + "ms");
            return 0;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            std::ofstream f(result_file);
            f << "TIMEOUT\n";
            f.close();
            log("child orphan_detect: timed out");
            return 1;
        }
        sleep_ms(poll_ms);
    }
}

int run_child_lock_claim(const std::filesystem::path& canvas_path) {
    log("child lock_claim: path=" + canvas_path.string() +
        " pid=" + std::to_string(my_pid()));
    if (!PublishedCanvases::try_acquire_lock(canvas_path)) {
        log("child lock_claim: try_acquire_lock failed");
        std::cout << "DENIED" << std::endl;
        std::cout.flush();
        return 2;
    }
    std::cout << "ACQUIRED " << my_pid() << std::endl;
    std::cout.flush();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "STOP") {
            PublishedCanvases::release_lock(canvas_path);
            std::cout << "RELEASED" << std::endl;
            std::cout.flush();
            return 0;
        }
    }
    log("child lock_claim: stdin closed, releasing and exiting");
    PublishedCanvases::release_lock(canvas_path);
    return 0;
}

// ---- parent-side helpers ------------------------------------------------

SDL_Process* spawn_self(const std::vector<std::string>& extra_args,
                        bool pipe_stdio) {
    if (g_self_exe.empty()) {
        log("spawn_self: g_self_exe is empty (set_self_exe_path not called?)");
        return nullptr;
    }
    std::vector<const char*> argv;
    argv.push_back(g_self_exe.c_str());
    for (auto& a : extra_args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    SDL_Process* p = SDL_CreateProcess(argv.data(), pipe_stdio);
    if (!p) {
        log(std::string("SDL_CreateProcess failed: ") + SDL_GetError());
    }
    return p;
}

int child_pid_of(SDL_Process* p) {
    SDL_PropertiesID props = SDL_GetProcessProperties(p);
    return static_cast<int>(SDL_GetNumberProperty(
        props, SDL_PROP_PROCESS_PID_NUMBER, 0));
}

// Read from a non-blocking IOStream until `needle` appears in the
// accumulated output, or `deadline` is reached. Returns the full
// accumulated string regardless of outcome.
std::string read_until(SDL_IOStream* out, const std::string& needle,
                       std::chrono::steady_clock::time_point deadline) {
    std::string acc;
    char buf[256];
    while (std::chrono::steady_clock::now() < deadline) {
        size_t n = SDL_ReadIO(out, buf, sizeof(buf));
        if (n > 0) acc.append(buf, buf + n);
        if (acc.find(needle) != std::string::npos) return acc;
        if (n == 0) sleep_ms(50);
    }
    return acc;
}

// ---- parent-side test bodies -------------------------------------------

int test_spawn_roundtrip() {
    log("=== test_spawn_roundtrip ===");
    SDL_Process* p = spawn_self({"--test-child-exit-zero"}, false);
    if (!p) return 1;
    int exit_code = -1;
    if (!SDL_WaitProcess(p, true, &exit_code)) {
        log(std::string("FAIL: SDL_WaitProcess: ") + SDL_GetError());
        SDL_DestroyProcess(p);
        return 1;
    }
    SDL_DestroyProcess(p);
    if (exit_code != 0) {
        log("FAIL: child exit_code=" + std::to_string(exit_code));
        return 1;
    }
    log("PASS");
    return 0;
}

int test_spawn_kill() {
    log("=== test_spawn_kill ===");
    SDL_Process* p = spawn_self({"--test-child-loop-forever"}, false);
    if (!p) return 1;
    int child = child_pid_of(p);
    log("child pid=" + std::to_string(child));
    sleep_ms(500);
    if (!process_alive(child)) {
        log("FAIL: child not alive after spawn");
        SDL_DestroyProcess(p);
        return 1;
    }
    if (!SDL_KillProcess(p, true)) {
        log(std::string("FAIL: SDL_KillProcess: ") + SDL_GetError());
        SDL_DestroyProcess(p);
        return 1;
    }
    int exit_code = -1;
    SDL_WaitProcess(p, true, &exit_code);
    SDL_DestroyProcess(p);
    // Small grace window for the OS to fully reap before we recheck.
    sleep_ms(100);
    if (process_alive(child)) {
        log("FAIL: child still alive after kill");
        return 1;
    }
    log("PASS (exit_code=" + std::to_string(exit_code) + ")");
    return 0;
}

int test_spawn_stdin_stop() {
    log("=== test_spawn_stdin_stop ===");
    SDL_Process* p = spawn_self({"--test-child-stdin-loop"}, true);
    if (!p) return 1;
    SDL_IOStream* in  = SDL_GetProcessInput(p);
    SDL_IOStream* out = SDL_GetProcessOutput(p);
    if (!in || !out) {
        log("FAIL: stdio not available");
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    // Wait for READY so we know the child is in its stdin loop.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::string acc = read_until(out, "READY", deadline);
    if (acc.find("READY") == std::string::npos) {
        log("FAIL: never saw READY. stdout: " + acc);
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    log("child reported READY");
    const std::string stop = "STOP\n";
    if (SDL_WriteIO(in, stop.data(), stop.size()) != stop.size()) {
        log(std::string("FAIL: write stdin: ") + SDL_GetError());
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    SDL_FlushIO(in);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    acc = read_until(out, "STOPPED", deadline);
    int exit_code = -1;
    SDL_WaitProcess(p, true, &exit_code);
    SDL_DestroyProcess(p);
    if (acc.find("STOPPED") == std::string::npos) {
        log("FAIL: never saw STOPPED. stdout: " + acc);
        return 1;
    }
    if (exit_code != 0) {
        log("FAIL: child exit_code=" + std::to_string(exit_code));
        return 1;
    }
    log("PASS");
    return 0;
}

int test_spawn_multi(int n) {
    log("=== test_spawn_multi N=" + std::to_string(n) + " ===");
    std::vector<SDL_Process*> ps;
    std::vector<int> pids;
    for (int i = 0; i < n; i++) {
        SDL_Process* p = spawn_self({"--test-child-loop-forever"}, false);
        if (!p) {
            log("FAIL: spawn #" + std::to_string(i));
            for (auto* q : ps) { SDL_KillProcess(q, true); SDL_DestroyProcess(q); }
            return 1;
        }
        ps.push_back(p);
        pids.push_back(child_pid_of(p));
    }
    sleep_ms(300);
    for (size_t i = 0; i < pids.size(); i++) {
        if (!process_alive(pids[i])) {
            log("FAIL: child #" + std::to_string(i) +
                " (pid " + std::to_string(pids[i]) + ") not alive");
            for (auto* q : ps) { SDL_KillProcess(q, true); SDL_DestroyProcess(q); }
            return 1;
        }
    }
    for (auto* p : ps) SDL_KillProcess(p, true);
    for (auto* p : ps) {
        int ec;
        SDL_WaitProcess(p, true, &ec);
        SDL_DestroyProcess(p);
    }
    sleep_ms(100);
    for (size_t i = 0; i < pids.size(); i++) {
        if (process_alive(pids[i])) {
            log("FAIL: child #" + std::to_string(i) + " still alive after kill");
            return 1;
        }
    }
    log("PASS (N=" + std::to_string(n) + ")");
    return 0;
}

// Phase 1 of the orphan-detect test: spawn the child, print its PID to
// stdout (so a test runner can capture it), let the child set up its
// poll loop, then this process returns — its caller (the test runner or
// outer wrapper) exits, orphaning the child. The child observes the
// parent gone, writes DETECTED to the result file, and exits.
//
// Phase 2 (--verify-orphan-detect) checks the artifact externally.
//
// The two phases are split because a single-process test can't both
// "die unexpectedly" and "report PASS/FAIL." The split keeps each phase
// honest about what it verifies.
int test_spawn_orphan_detect(const std::string& result_file) {
    log("=== test_spawn_orphan_detect result=" + result_file + " ===");
    std::error_code ec;
    std::filesystem::remove(result_file, ec);

    int parent_pid = my_pid();
    SDL_Process* p = spawn_self({
        "--test-child-orphan-detect",
        std::to_string(parent_pid),
        result_file
    }, false);
    if (!p) return 1;
    int child = child_pid_of(p);
    // Detach the SDL_Process record — we want the OS-level child to
    // outlive this process, not be reaped on SDL_DestroyProcess.
    SDL_DestroyProcess(p);

    sleep_ms(500);  // give child time to enter its poll loop
    log("orphan_detect phase 1 done. child_pid=" + std::to_string(child));
    log("verify with:  inkternity --verify-orphan-detect " +
        std::to_string(child) + " " + result_file);
    // Print the child PID on stdout for capture by a test runner script.
    std::cout << "CHILD_PID " << child << std::endl;
    std::cout.flush();
    return 0;
}

int verify_orphan_detect(int child_pid, const std::string& result_file,
                         int timeout_sec) {
    log("=== verify_orphan_detect child_pid=" + std::to_string(child_pid) +
        " result=" + result_file + " timeout=" +
        std::to_string(timeout_sec) + "s ===");
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(timeout_sec);
    while (std::chrono::steady_clock::now() < deadline) {
        const bool result_exists = std::filesystem::exists(result_file);
        const bool child_alive = process_alive(child_pid);
        if (result_exists && !child_alive) {
            std::ifstream f(result_file);
            std::string line;
            std::getline(f, line);
            if (line.rfind("DETECTED", 0) == 0) {
                log("PASS: " + line);
                return 0;
            }
            log("FAIL: result file contains: " + line);
            return 1;
        }
        sleep_ms(200);
    }
    log("FAIL: timed out. child_alive=" +
        std::string(process_alive(child_pid) ? "yes" : "no") +
        " result_exists=" +
        std::string(std::filesystem::exists(result_file) ? "yes" : "no"));
    return 1;
}

int test_lock_handoff(const std::filesystem::path& canvas_path) {
    log("=== test_lock_handoff path=" + canvas_path.string() + " ===");
    // Defensive cleanup so a prior failed run doesn't poison this one.
    PublishedCanvases::release_lock(canvas_path);
    std::error_code ec;
    std::filesystem::remove(canvas_path.string() + ".lock", ec);

    SDL_Process* p = spawn_self({
        "--test-child-lock-claim",
        canvas_path.string()
    }, true);
    if (!p) return 1;
    SDL_IOStream* in  = SDL_GetProcessInput(p);
    SDL_IOStream* out = SDL_GetProcessOutput(p);
    if (!in || !out) {
        log("FAIL: no stdio");
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::string acc = read_until(out, "ACQUIRED", deadline);
    if (acc.find("ACQUIRED") == std::string::npos) {
        log("FAIL: never saw ACQUIRED. stdout: " + acc);
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    log("child reported: " + acc);

    if (!PublishedCanvases::is_locked_by_anyone(canvas_path)) {
        log("FAIL: is_locked_by_anyone returned false after ACQUIRED");
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    log("PublishedCanvases sees the lock as held");

    const std::string stop = "STOP\n";
    if (SDL_WriteIO(in, stop.data(), stop.size()) != stop.size()) {
        log("FAIL: write stdin");
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    SDL_FlushIO(in);

    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    acc = read_until(out, "RELEASED", deadline);
    int exit_code = -1;
    SDL_WaitProcess(p, true, &exit_code);
    SDL_DestroyProcess(p);

    if (acc.find("RELEASED") == std::string::npos) {
        log("FAIL: never saw RELEASED. stdout: " + acc);
        return 1;
    }
    if (exit_code != 0) {
        log("FAIL: child exit_code=" + std::to_string(exit_code));
        return 1;
    }
    sleep_ms(100);
    if (PublishedCanvases::is_locked_by_anyone(canvas_path)) {
        log("FAIL: lock still held after child exited");
        return 1;
    }
    log("PASS");
    return 0;
}

// Spawn the real --host-only side-instance against a canvas file,
// drive it through the READY → idle → STOP → STOPPING → exit
// lifecycle, and verify the lock was released cleanly. This is the
// smoke test for the actual hosting code path (NetLibrary::init,
// World::start_hosting, idle loop, graceful shutdown) — distinct
// from --test-lock-handoff which only verifies the lock primitive
// against a stub child.
int test_host_only_roundtrip(const std::filesystem::path& canvas_path,
                             int idle_seconds) {
    log("=== test_host_only_roundtrip path=" + canvas_path.string() +
        " idle=" + std::to_string(idle_seconds) + "s ===");
    if (!std::filesystem::exists(canvas_path)) {
        log("FAIL: canvas file does not exist: " + canvas_path.string());
        return 1;
    }
    // Defensive: clear any stale lock from a prior aborted run.
    PublishedCanvases::release_lock(canvas_path);
    std::error_code ec;
    std::filesystem::remove(canvas_path.string() + ".lock", ec);

    SDL_Process* p = spawn_self({
        "--host-only",
        canvas_path.string()
    }, true);
    if (!p) return 1;
    SDL_IOStream* in  = SDL_GetProcessInput(p);
    SDL_IOStream* out = SDL_GetProcessOutput(p);
    if (!in || !out) {
        log("FAIL: no stdio");
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }

    // Phase 1: wait for READY (host has start_hosting'd successfully).
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    std::string acc = read_until(out, "READY", deadline);
    if (acc.find("READY") == std::string::npos) {
        log("FAIL: never saw READY within 30s. stdout: " + acc);
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    log("host reported READY (start_hosting succeeded)");

    // Verify the lock is held by the side-instance.
    if (!PublishedCanvases::is_locked_by_anyone(canvas_path)) {
        log("FAIL: is_locked_by_anyone returned false after READY");
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    log("lock confirmed held by side-instance");

    // Phase 2: let it idle so NetLibrary::update / world->update tick
    // for a while. Real subscriber-paint testing happens externally;
    // this just verifies the loop doesn't crash on no-traffic ticks.
    log("idling " + std::to_string(idle_seconds) + "s to exercise the loop");
    sleep_ms(idle_seconds * 1000);
    {
        int dummy;
        if (SDL_WaitProcess(p, false, &dummy)) {
            log("FAIL: side-instance unexpectedly exited during idle "
                "(exit_code=" + std::to_string(dummy) + ")");
            SDL_DestroyProcess(p);
            return 1;
        }
    }

    // Phase 3: send STOP, expect STOPPING + clean exit.
    const std::string stop = "STOP\n";
    if (SDL_WriteIO(in, stop.data(), stop.size()) != stop.size()) {
        log("FAIL: write stdin");
        SDL_KillProcess(p, true);
        SDL_DestroyProcess(p);
        return 1;
    }
    SDL_FlushIO(in);

    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    acc = read_until(out, "STOPPING", deadline);
    int exit_code = -1;
    SDL_WaitProcess(p, true, &exit_code);
    SDL_DestroyProcess(p);

    if (acc.find("STOPPING") == std::string::npos) {
        log("FAIL: never saw STOPPING after STOP. stdout: " + acc);
        return 1;
    }
    if (exit_code != 0) {
        log("FAIL: host exit_code=" + std::to_string(exit_code));
        return 1;
    }
    sleep_ms(200);
    if (PublishedCanvases::is_locked_by_anyone(canvas_path)) {
        log("FAIL: lock still held after side-instance exited");
        return 1;
    }
    log("PASS");
    return 0;
}

// Exercise the SideInstances manager directly: construct, spawn for
// one real canvas, verify lock is taken by the child + is_alive +
// is_managing, sleep idle, stop_all via dtor scope, verify lock and
// process are both gone. Distinct from --test-host-only-roundtrip
// (which exercises just the side-instance binary): this one exercises
// the *manager* code path the main process will use at launch.
int test_side_instances(const std::filesystem::path& canvas_path,
                        int idle_seconds) {
    log("=== test_side_instances path=" + canvas_path.string() +
        " idle=" + std::to_string(idle_seconds) + "s ===");
    if (!std::filesystem::exists(canvas_path)) {
        log("FAIL: canvas does not exist: " + canvas_path.string());
        return 1;
    }
    PublishedCanvases::release_lock(canvas_path);
    std::error_code ec;
    std::filesystem::remove(canvas_path.string() + ".lock", ec);

    // SideInstances logs through Logger, which throws if no handler
    // for the log type is registered. In normal startup MainProgram
    // wires WORLDFATAL/USERINFO/CHAT; INFO is wired by main.cpp's
    // init_logs and FATAL by Logger users elsewhere. In test mode
    // those handlers aren't installed because we bypass MainProgram —
    // wire fallback stderr handlers so SideInstances logs don't throw.
    Logger::get().add_log("INFO", [](const std::string& s) {
        std::cerr << "[INFO] " << s << std::endl;
    });
    Logger::get().add_log("WORLDFATAL", [](const std::string& s) {
        std::cerr << "[WORLDFATAL] " << s << std::endl;
    });

    int rc = 0;
    {
        SideInstances mgr(g_self_exe);
        if (!mgr.spawn(canvas_path)) {
            log("FAIL: spawn returned false");
            return 1;
        }
        if (!mgr.is_managing(canvas_path)) {
            log("FAIL: is_managing returned false after spawn");
            return 1;
        }
        log("manager reports is_managing=true");


        // Side-instance needs ~1-2s to acquire its lock (Logger +
        // FontData + DevKeys + try_acquire_lock chain). Poll for up
        // to 5s.
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(5);
        bool lock_held = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (PublishedCanvases::is_locked_by_anyone(canvas_path)) {
                lock_held = true;
                break;
            }
            sleep_ms(100);
        }
        if (!lock_held) {
            log("FAIL: side-instance did not acquire lock within 5s");
            return 1;
        }
        log("side-instance acquired lock");
        if (!mgr.is_alive(canvas_path)) {
            log("FAIL: is_alive returned false after lock acquired");
            return 1;
        }
        sleep_ms(idle_seconds * 1000);
        if (!mgr.is_alive(canvas_path)) {
            log("FAIL: side-instance died during idle");
            return 1;
        }
        log("manager state stable through " +
            std::to_string(idle_seconds) + "s idle");
        // Leaving scope -> mgr.~SideInstances() -> stop_all().
    }

    // Post-dtor: lock should be released, no managed processes left.
    sleep_ms(200);
    if (PublishedCanvases::is_locked_by_anyone(canvas_path)) {
        log("FAIL: lock still held after manager dtor");
        rc = 1;
    } else {
        log("manager dtor released lock cleanly");
    }
    if (rc == 0) log("PASS");
    return rc;
}

// Exercise the launch-time scan-and-spawn flow: scan a saves dir,
// spawn a side-instance for each unlocked marker, verify all became
// alive + locked, idle, then dtor stops all.
//
// Requires the caller to have set up <savesDir> with the canvases +
// .publish markers ahead of time. Use the Toolbar's "Publish" toggle
// in the running app to create markers, or `touch` them next to the
// canvas files for a synthetic test.
int test_scan_and_spawn(const std::filesystem::path& saves_dir,
                        int idle_seconds) {
    log("=== test_scan_and_spawn dir=" + saves_dir.string() +
        " idle=" + std::to_string(idle_seconds) + "s ===");
    if (!std::filesystem::is_directory(saves_dir)) {
        log("FAIL: not a directory: " + saves_dir.string());
        return 1;
    }
    // Register Logger fallbacks (same reason as test_side_instances).
    Logger::get().add_log("INFO", [](const std::string& s) {
        std::cerr << "[INFO] " << s << std::endl;
    });
    Logger::get().add_log("WORLDFATAL", [](const std::string& s) {
        std::cerr << "[WORLDFATAL] " << s << std::endl;
    });

    int rc = 0;
    {
        SideInstances mgr(g_self_exe);
        const size_t spawned = mgr.scan_and_spawn(saves_dir);
        if (spawned == 0) {
            log("FAIL: scan_and_spawn returned 0 — no markers in " +
                saves_dir.string() + "? (Create one via the Toolbar "
                "Publish toggle, or touch a .publish sidecar)");
            return 1;
        }
        log("scan_and_spawn spawned " + std::to_string(spawned) +
            " side-instance(s)");

        // Wait for each managed canvas to acquire its lock.
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(10);
        for (const auto& canvasPath : mgr.managed()) {
            while (std::chrono::steady_clock::now() < deadline) {
                if (PublishedCanvases::is_locked_by_anyone(canvasPath)) break;
                sleep_ms(100);
            }
            if (!PublishedCanvases::is_locked_by_anyone(canvasPath)) {
                log("FAIL: " + canvasPath.filename().string() +
                    " never acquired lock");
                return 1;
            }
            log("locked: " + canvasPath.filename().string());
        }

        sleep_ms(idle_seconds * 1000);
        for (const auto& canvasPath : mgr.managed()) {
            if (!mgr.is_alive(canvasPath)) {
                log("FAIL: " + canvasPath.filename().string() +
                    " died during idle");
                return 1;
            }
        }
        log("all side-instances alive after " +
            std::to_string(idle_seconds) + "s idle");
        // Leaving scope → dtor → stop_all.
    }
    sleep_ms(200);
    log("PASS");
    return rc;
}

int test_all_spawn() {
    log("=== test_all_spawn (deterministic suite) ===");
    int rc = 0;
    if (test_spawn_roundtrip()  != 0) rc = 1;
    if (test_spawn_kill()       != 0) rc = 1;
    if (test_spawn_stdin_stop() != 0) rc = 1;
    if (test_spawn_multi(5)     != 0) rc = 1;
    log(rc == 0 ? "ALL SPAWN TESTS PASS" : "SUITE FAILED");
    return rc;
}

}  // anonymous namespace

// ---- public API ---------------------------------------------------------

std::string resolve_self_exe(const char* argv0) {
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, sizeof(buf) / sizeof(buf[0]));
    if (n > 0 && n < sizeof(buf) / sizeof(buf[0])) {
        return std::filesystem::path(std::wstring(buf, n)).string();
    }
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        std::error_code ec;
        auto canon = std::filesystem::canonical(buf, ec);
        if (!ec) return canon.string();
        return buf;
    }
#elif defined(__linux__)
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        return buf;
    }
#endif
    return argv0 ? std::string(argv0) : std::string();
}

void set_self_exe_path(const std::string& path) { g_self_exe = path; }
const std::string& self_exe_path() { return g_self_exe; }

std::optional<int> dispatch(int argc, char** argv) {
    if (argc < 2) return std::nullopt;
    const std::string flag(argv[1]);

    // --- child-side tests (single invocation, no spawn) ---
    if (flag == "--test-child-exit-zero")    return run_child_exit_zero();
    if (flag == "--test-child-loop-forever") return run_child_loop_forever();
    if (flag == "--test-child-stdin-loop")   return run_child_stdin_loop();
    if (flag == "--test-child-orphan-detect") {
        if (argc < 4) {
            log("usage: --test-child-orphan-detect <parent-pid> <result-file>");
            return 2;
        }
        return run_child_orphan_detect(std::atoi(argv[2]), argv[3]);
    }
    if (flag == "--test-child-lock-claim") {
        if (argc < 3) {
            log("usage: --test-child-lock-claim <canvas-path>");
            return 2;
        }
        return run_child_lock_claim(argv[2]);
    }

    // --- parent-side tests (spawn children) ---
    if (flag == "--test-spawn-roundtrip")  return test_spawn_roundtrip();
    if (flag == "--test-spawn-kill")       return test_spawn_kill();
    if (flag == "--test-spawn-stdin-stop") return test_spawn_stdin_stop();
    if (flag == "--test-spawn-multi") {
        int n = (argc >= 3) ? std::atoi(argv[2]) : 5;
        return test_spawn_multi(n);
    }
    if (flag == "--test-spawn-orphan-detect") {
        if (argc < 3) {
            log("usage: --test-spawn-orphan-detect <result-file>");
            return 2;
        }
        return test_spawn_orphan_detect(argv[2]);
    }
    if (flag == "--verify-orphan-detect") {
        if (argc < 4) {
            log("usage: --verify-orphan-detect <child-pid> <result-file> "
                "[<timeout-sec>]");
            return 2;
        }
        int timeout = (argc >= 5) ? std::atoi(argv[4]) : 30;
        return verify_orphan_detect(std::atoi(argv[2]), argv[3], timeout);
    }
    if (flag == "--test-lock-handoff") {
        if (argc < 3) {
            log("usage: --test-lock-handoff <canvas-path>");
            return 2;
        }
        return test_lock_handoff(argv[2]);
    }
    if (flag == "--test-host-only-roundtrip") {
        if (argc < 3) {
            log("usage: --test-host-only-roundtrip <canvas-path> "
                "[<idle-seconds>]");
            return 2;
        }
        int idle = (argc >= 4) ? std::atoi(argv[3]) : 2;
        return test_host_only_roundtrip(argv[2], idle);
    }
    if (flag == "--test-side-instances") {
        if (argc < 3) {
            log("usage: --test-side-instances <canvas-path> "
                "[<idle-seconds>]");
            return 2;
        }
        int idle = (argc >= 4) ? std::atoi(argv[3]) : 2;
        return test_side_instances(argv[2], idle);
    }
    if (flag == "--test-scan-and-spawn") {
        if (argc < 3) {
            log("usage: --test-scan-and-spawn <saves-dir> "
                "[<idle-seconds>]");
            return 2;
        }
        int idle = (argc >= 4) ? std::atoi(argv[3]) : 2;
        return test_scan_and_spawn(argv[2], idle);
    }
    if (flag == "--test-all-spawn") return test_all_spawn();

    return std::nullopt;
}

}  // namespace ProcessTests
