#include "SideInstances.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "../PublishedCanvases.hpp"
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
    #include <sys/types.h>
    #include <unistd.h>
#endif

namespace {

int my_pid() {
#ifdef _WIN32
    return static_cast<int>(_getpid());
#else
    return static_cast<int>(getpid());
#endif
}

// Same canonicalisation PublishedCanvases uses for its in-memory
// held-locks set, so the two stay in sync.
std::string canonical_key(const std::filesystem::path& canvasPath) {
    std::error_code ec;
    auto p = std::filesystem::weakly_canonical(canvasPath, ec);
    return (ec ? canvasPath.lexically_normal() : p).string();
}

void log_info(const std::string& s) {
    Logger::get().log("INFO", "[SideInstances] " + s);
}

void log_fatal(const std::string& s) {
    Logger::get().log("WORLDFATAL", "[SideInstances] " + s);
}

void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

struct ManagedInstance {
    SDL_Process* process = nullptr;
    std::filesystem::path canvasPath;
};

}  // anonymous

struct SideInstances::Impl {
    std::string selfExePath;
    std::unordered_map<std::string /*canonical key*/, ManagedInstance> instances;
};

SideInstances::SideInstances(const std::string& selfExePath)
    : impl(std::make_unique<Impl>())
{
    impl->selfExePath = selfExePath;
}

SideInstances::~SideInstances() {
    stop_all();
}

bool SideInstances::spawn(const std::filesystem::path& canvasPath) {
    if (impl->selfExePath.empty()) {
        log_fatal("spawn(" + canvasPath.string() +
                  "): self-exe path is empty; cannot spawn");
        return false;
    }
    const auto key = canonical_key(canvasPath);
    if (impl->instances.count(key)) {
        log_info("spawn(" + canvasPath.string() +
                 "): already managing, no-op");
        return true;
    }

    const std::string parentPidStr = std::to_string(my_pid());
    const std::string pathStr = canvasPath.string();
    std::vector<const char*> argv;
    argv.push_back(impl->selfExePath.c_str());
    argv.push_back("--host-only");
    argv.push_back(pathStr.c_str());
    argv.push_back("--parent-pid");
    argv.push_back(parentPidStr.c_str());
    argv.push_back(nullptr);

    // We need stdin piped (so we can write "STOP\n" to the child to
    // request graceful shutdown), but stdout/stderr MUST NOT be piped
    // to APP — otherwise the side-instance's log writes accumulate in
    // an undrained kernel pipe buffer and eventually block. The
    // side-instance writes its own per-canvas log file
    // (`<configPath>/logs/host-<name>.log`), so dropping stdout/stderr
    // here loses nothing operationally.
    SDL_PropertiesID createProps = SDL_CreateProperties();
    SDL_SetPointerProperty(createProps, SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
                           (void*)argv.data());
    SDL_SetNumberProperty(createProps, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
                          SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(createProps, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
                          SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(createProps, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
                          SDL_PROCESS_STDIO_NULL);
    SDL_Process* p = SDL_CreateProcessWithProperties(createProps);
    SDL_DestroyProperties(createProps);
    if (!p) {
        log_fatal(std::string("SDL_CreateProcessWithProperties failed for ") +
                  canvasPath.string() + ": " + SDL_GetError());
        return false;
    }

    ManagedInstance mi;
    mi.process = p;
    mi.canvasPath = canvasPath;
    impl->instances.emplace(key, std::move(mi));

    SDL_PropertiesID props = SDL_GetProcessProperties(p);
    const Sint64 pid = SDL_GetNumberProperty(
        props, SDL_PROP_PROCESS_PID_NUMBER, 0);
    log_info("spawned side-instance pid=" + std::to_string(pid) +
             " for " + canvasPath.string());
    return true;
}

size_t SideInstances::scan_and_spawn(const std::filesystem::path& savesDir) {
    const auto candidates = PublishedCanvases::scan_published(savesDir);
    size_t spawned = 0;
    for (const auto& canvasPath : candidates) {
        if (PublishedCanvases::is_locked_by_anyone(canvasPath)) {
            log_info("scan: " + canvasPath.filename().string() +
                     " already locked, skipping");
            continue;
        }
        if (spawn(canvasPath)) {
            spawned++;
        }
    }
    log_info("scan_and_spawn(" + savesDir.string() +
             "): " + std::to_string(candidates.size()) +
             " marker(s), " + std::to_string(spawned) + " spawned");
    return spawned;
}

bool SideInstances::stop(const std::filesystem::path& canvasPath) {
    const auto key = canonical_key(canvasPath);
    auto it = impl->instances.find(key);
    if (it == impl->instances.end()) return false;

    SDL_Process* p = it->second.process;
    log_info("stop(" + canvasPath.string() + "): sending STOP");

    // Phase 1: politely ask the side-instance to exit via stdin.
    SDL_IOStream* in = SDL_GetProcessInput(p);
    if (in) {
        const std::string stop = "STOP\n";
        SDL_WriteIO(in, stop.data(), stop.size());
        SDL_FlushIO(in);
    } else {
        log_info("stop(" + canvasPath.string() +
                 "): no stdin pipe (oddly) — skipping graceful path");
    }

    // Phase 2: wait for graceful exit up to STOP_TIMEOUT.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(STOP_TIMEOUT_SECONDS);
    int exit_code = -1;
    bool exited = false;
    while (std::chrono::steady_clock::now() < deadline) {
        if (SDL_WaitProcess(p, /*block=*/false, &exit_code)) {
            exited = true;
            break;
        }
        sleep_ms(50);
    }

    // Phase 3: force-kill if the side-instance ignored STOP. Reasonable
    // for Phase 1; the lock file will be reclaimed by the next acquire
    // attempt via stale-PID detection if we had to kill mid-write.
    if (!exited) {
        log_info("stop(" + canvasPath.string() +
                 "): timeout, force-killing");
        SDL_KillProcess(p, /*force=*/true);
        SDL_WaitProcess(p, /*block=*/true, &exit_code);
    }

    SDL_DestroyProcess(p);
    impl->instances.erase(it);
    log_info("stop(" + canvasPath.string() + "): exit_code=" +
             std::to_string(exit_code) +
             (exited ? " (graceful)" : " (killed)"));
    return true;
}

void SideInstances::stop_all() {
    if (impl->instances.empty()) return;

    log_info("stop_all: " + std::to_string(impl->instances.size()) +
             " side-instance(s)");

    // Two-pass shutdown: send STOP to every instance first (so they all
    // start flushing in parallel), then wait on each one with timeout.
    // Sequential stop() would force each side-instance to flush before
    // the next gets the signal — N × ~1s instead of ~1s total.

    // Pass 1: signal all.
    for (auto& [_key, mi] : impl->instances) {
        SDL_IOStream* in = SDL_GetProcessInput(mi.process);
        if (in) {
            const std::string stop = "STOP\n";
            SDL_WriteIO(in, stop.data(), stop.size());
            SDL_FlushIO(in);
        }
    }

    // Pass 2: wait + kill stragglers. Uses a single deadline across all
    // processes — well-behaved side-instances should all finish in <1s
    // since they're flushing in parallel; the timeout is a backstop.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(STOP_TIMEOUT_SECONDS);
    while (!impl->instances.empty()) {
        for (auto it = impl->instances.begin(); it != impl->instances.end(); ) {
            int exit_code = -1;
            if (SDL_WaitProcess(it->second.process, false, &exit_code)) {
                SDL_DestroyProcess(it->second.process);
                it = impl->instances.erase(it);
            } else {
                ++it;
            }
        }
        if (impl->instances.empty()) break;
        if (std::chrono::steady_clock::now() >= deadline) break;
        sleep_ms(50);
    }

    // Anything still in the map ignored STOP. Force-kill.
    for (auto& [_key, mi] : impl->instances) {
        log_info("stop_all: force-killing " + mi.canvasPath.string());
        SDL_KillProcess(mi.process, /*force=*/true);
        int exit_code = -1;
        SDL_WaitProcess(mi.process, /*block=*/true, &exit_code);
        SDL_DestroyProcess(mi.process);
    }
    impl->instances.clear();
    log_info("stop_all: done");
}

bool SideInstances::is_managing(const std::filesystem::path& canvasPath) const {
    return impl->instances.count(canonical_key(canvasPath)) > 0;
}

std::vector<std::filesystem::path> SideInstances::managed() const {
    std::vector<std::filesystem::path> out;
    out.reserve(impl->instances.size());
    for (const auto& [_key, mi] : impl->instances) {
        out.emplace_back(mi.canvasPath);
    }
    return out;
}

bool SideInstances::is_alive(const std::filesystem::path& canvasPath) const {
    const auto key = canonical_key(canvasPath);
    auto it = impl->instances.find(key);
    if (it == impl->instances.end()) return false;
    int exit_code = -1;
    // SDL_WaitProcess(block=false) returns true if the process has
    // exited (whether successfully or not). Inverted: still alive iff
    // it returns false.
    return !SDL_WaitProcess(it->second.process, false, &exit_code);
}
