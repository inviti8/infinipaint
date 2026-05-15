#include "PublishRegistry.hpp"
#include <Helpers/Logger.hpp>
#include <Helpers/StringHelpers.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace {

constexpr const char* FILE_NAME = "inkternity_published.json";

// Returns the current UTC time in ISO-8601 (e.g. "2026-05-14T18:30:00Z").
// Used as the publishedAt stamp; intended for sort/display only — not a
// load-bearing field.
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

}  // namespace

bool PublishRegistry::load(const std::filesystem::path& configPath) {
    entry.reset();
    const auto path = configPath / FILE_NAME;
    std::string fileData;
    try {
        fileData = read_file_to_string(path);
    } catch (...) {
        // Absent file is the natural starting state — nothing published.
        return true;
    }

    try {
        const auto j = nlohmann::json::parse(fileData);
        if (j.contains("published") && j["published"].is_object()) {
            const auto& p = j["published"];
            Entry e;
            e.path = std::filesystem::path(p.value("path", ""));
            e.publishedAt = p.value("publishedAt", "");
            if (!e.path.empty()) entry = std::move(e);
        }
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO",
            std::string("PublishRegistry: parse failed, treating as empty: ") + e.what());
        return false;
    }
    return true;
}

bool PublishRegistry::is_published(const std::filesystem::path& path) const {
    if (!entry) return false;
    std::error_code ec;
    auto a = std::filesystem::weakly_canonical(entry->path, ec);
    if (ec) a = entry->path.lexically_normal();
    auto b = std::filesystem::weakly_canonical(path, ec);
    if (ec) b = path.lexically_normal();
    return a == b;
}

bool PublishRegistry::set_published(const std::filesystem::path& path,
                                     const std::filesystem::path& configPath) {
    Entry e;
    std::error_code ec;
    e.path = std::filesystem::weakly_canonical(path, ec);
    if (ec) e.path = path.lexically_normal();
    e.publishedAt = now_iso8601_utc();
    entry = std::move(e);
    return save(configPath);
}

bool PublishRegistry::clear(const std::filesystem::path& configPath) {
    entry.reset();
    return save(configPath);
}

bool PublishRegistry::save(const std::filesystem::path& configPath) {
    const auto path = configPath / FILE_NAME;
    nlohmann::json j;
    j["version"] = 1;
    if (entry) {
        j["published"] = {
            {"path", entry->path.string()},
            {"publishedAt", entry->publishedAt}
        };
    }
    try {
        std::ofstream(path) << j.dump(2);
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO",
            std::string("PublishRegistry: write failed: ") + e.what());
        return false;
    }
    return true;
}
