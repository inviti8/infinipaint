#include "PublishedCanvases.hpp"
#include <Helpers/Logger.hpp>
#include <Helpers/StringHelpers.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <process.h>  // _getpid
#else
    #include <cerrno>
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

namespace PublishedCanvases {

namespace {

// Sidecar file extensions appended to the canvas filename.
constexpr const char* MARKER_SUFFIX = ".publish";
constexpr const char* LOCK_SUFFIX   = ".lock";

// Tracks the locks this process is currently holding so we can:
//   - answer is_locked_by_us() without filesystem I/O
//   - release_all_held() at graceful shutdown
// Using path strings (not paths) for canonical comparison hygiene.
std::mutex& held_lock_mutex() {
    static std::mutex m;
    return m;
}
std::set<std::string>& held_locks() {
    static std::set<std::string> s;
    return s;
}

std::filesystem::path marker_path(const std::filesystem::path& canvasPath) {
    return canvasPath.string() + MARKER_SUFFIX;
}
std::filesystem::path lock_path(const std::filesystem::path& canvasPath) {
    return canvasPath.string() + LOCK_SUFFIX;
}

// Canonical key for held_locks set. Uses weakly_canonical so slash
// direction / relative paths normalize before set lookup.
std::string canonical_key(const std::filesystem::path& canvasPath) {
    std::error_code ec;
    auto p = std::filesystem::weakly_canonical(canvasPath, ec);
    return (ec ? canvasPath.lexically_normal() : p).string();
}

std::string now_iso8601_utc() {
    using namespace std::chrono;
    const auto t = system_clock::to_time_t(system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

int current_pid() {
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

// True iff the OS reports a process with this PID currently exists.
// Used to detect stale lock files from crashed/killed instances.
bool pid_alive(int pid) {
    if (pid <= 0) return false;
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) return false;
    DWORD exitCode = 0;
    const bool stillRunning = GetExitCodeProcess(h, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(h);
    return stillRunning;
#else
    // kill(pid, 0) returns 0 if the process exists and we have permission
    // to signal it; -1 with errno=ESRCH if it doesn't exist; -1 with
    // errno=EPERM if it exists but we can't signal (treat as alive).
    if (::kill(pid, 0) == 0) return true;
    return errno == EPERM;
#endif
}

// Try to read a lock file's PID. Returns nullopt on missing/malformed.
std::optional<int> read_lock_pid(const std::filesystem::path& lockPath) {
    std::string data;
    try {
        data = read_file_to_string(lockPath);
    } catch (...) {
        return std::nullopt;
    }
    try {
        const auto j = nlohmann::json::parse(data);
        return j.value("pid", 0);
    } catch (...) {
        return std::nullopt;
    }
}

// Write `pid` into `lockPath` atomically *and exclusively* — fails if
// the file already exists. The atomicity is the core of the
// no-simultaneous-launch race guarantee: two instances calling this
// concurrently for the same canvas can't both succeed.
//
// POSIX: open(O_CREAT|O_EXCL|O_WRONLY).
// Windows: CreateFileA(CREATE_NEW).
//
// Returns true on success, false if the file already exists or on any
// other write error.
bool write_lock_exclusive(const std::filesystem::path& lockPath, int pid) {
    const std::string payload = nlohmann::json{{"pid", pid}}.dump();

#ifdef _WIN32
    HANDLE h = CreateFileW(lockPath.wstring().c_str(),
                            GENERIC_WRITE, 0, nullptr,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(h, payload.data(),
                               static_cast<DWORD>(payload.size()), &written, nullptr)
                    && written == payload.size();
    CloseHandle(h);
    if (!ok) {
        std::error_code ec;
        std::filesystem::remove(lockPath, ec);
    }
    return ok;
#else
    int fd = ::open(lockPath.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) return false;
    const ssize_t w = ::write(fd, payload.data(), payload.size());
    ::close(fd);
    if (w != static_cast<ssize_t>(payload.size())) {
        std::error_code ec;
        std::filesystem::remove(lockPath, ec);
        return false;
    }
    return true;
#endif
}

}  // namespace

// ---- per-canvas "published" marker --------------------------------------

bool is_published(const std::filesystem::path& canvasPath) {
    std::error_code ec;
    return std::filesystem::exists(marker_path(canvasPath), ec);
}

bool set_published(const std::filesystem::path& canvasPath) {
    nlohmann::json j;
    j["publishedAt"] = now_iso8601_utc();
    try {
        std::ofstream(marker_path(canvasPath)) << j.dump(2);
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO",
            std::string("PublishedCanvases::set_published: write failed: ") + e.what());
        return false;
    }
    return true;
}

bool clear_published(const std::filesystem::path& canvasPath) {
    std::error_code ec;
    std::filesystem::remove(marker_path(canvasPath), ec);
    // ec set on real failure (other than "didn't exist"); treat as success
    // unless it's something serious. Idempotent semantics.
    return true;
}

// ---- per-canvas runtime lock --------------------------------------------

bool try_acquire_lock(const std::filesystem::path& canvasPath) {
    const auto lp = lock_path(canvasPath);
    const int myPid = current_pid();

    // Fast path: no existing lock file → exclusive create + write.
    if (write_lock_exclusive(lp, myPid)) {
        std::lock_guard<std::mutex> g(held_lock_mutex());
        held_locks().insert(canonical_key(canvasPath));
        return true;
    }

    // Slow path: lock file exists. Check whether holder is alive.
    const auto holderPid = read_lock_pid(lp);
    if (holderPid.has_value() && *holderPid != myPid && pid_alive(*holderPid)) {
        return false;  // genuinely contested
    }

    // Stale (or our own zombie from a previous run): reclaim by removing
    // and re-creating exclusively. The remove + create is racy with another
    // launching instance, so the second exclusive-create-or-fail is the
    // tiebreaker.
    std::error_code ec;
    std::filesystem::remove(lp, ec);
    if (write_lock_exclusive(lp, myPid)) {
        std::lock_guard<std::mutex> g(held_lock_mutex());
        held_locks().insert(canonical_key(canvasPath));
        Logger::get().log("USERINFO",
            "PublishedCanvases: reclaimed stale lock " + lp.string() +
            (holderPid ? " (was pid " + std::to_string(*holderPid) + ")" : ""));
        return true;
    }
    return false;
}

void release_lock(const std::filesystem::path& canvasPath) {
    const auto key = canonical_key(canvasPath);
    {
        std::lock_guard<std::mutex> g(held_lock_mutex());
        if (!held_locks().erase(key)) return;  // we didn't hold it
    }
    std::error_code ec;
    std::filesystem::remove(lock_path(canvasPath), ec);
}

bool is_locked_by_anyone(const std::filesystem::path& canvasPath) {
    const auto lp = lock_path(canvasPath);
    std::error_code ec;
    if (!std::filesystem::exists(lp, ec)) return false;
    const auto holderPid = read_lock_pid(lp);
    if (!holderPid.has_value()) return false;
    return pid_alive(*holderPid);
}

bool is_locked_by_us(const std::filesystem::path& canvasPath) {
    std::lock_guard<std::mutex> g(held_lock_mutex());
    return held_locks().count(canonical_key(canvasPath)) > 0;
}

// ---- scanning -----------------------------------------------------------

std::vector<std::filesystem::path> scan_published(
    const std::filesystem::path& savesDir)
{
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    if (!std::filesystem::exists(savesDir, ec)) return out;

    for (const auto& entry : std::filesystem::directory_iterator(savesDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();
        const auto ext = p.extension().string();
        // Match .inkternity (canonical) or .infpnt (legacy) only —
        // skip thumbnails, sidecars, etc.
        if (ext != ".inkternity" && ext != ".infpnt") continue;
        if (is_published(p)) out.push_back(p);
    }
    return out;
}

std::optional<std::filesystem::path> claim_first_available(
    const std::filesystem::path& savesDir)
{
    for (const auto& p : scan_published(savesDir)) {
        if (try_acquire_lock(p)) return p;
    }
    return std::nullopt;
}

void release_all_held() {
    std::vector<std::string> snapshot;
    {
        std::lock_guard<std::mutex> g(held_lock_mutex());
        snapshot.assign(held_locks().begin(), held_locks().end());
        held_locks().clear();
    }
    for (const auto& k : snapshot) {
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path(k).string() + LOCK_SUFFIX, ec);
    }
}

}  // namespace PublishedCanvases
