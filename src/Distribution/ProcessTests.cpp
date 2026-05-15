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

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
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
    if (flag == "--test-all-spawn") return test_all_spawn();

    return std::nullopt;
}

}  // namespace ProcessTests
