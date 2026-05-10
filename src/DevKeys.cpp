#include "DevKeys.hpp"
#include <Helpers/Logger.hpp>
#include <Helpers/StringHelpers.hpp>
#include <nlohmann/json.hpp>

bool DevKeys::load(const std::filesystem::path& configPath) {
    const auto path = configPath / "inkternity_dev_keys.json";
    std::string fileData;
    try {
        fileData = read_file_to_string(path);
    } catch (...) {
        // Absence is normal — vanilla Inkternity has no dev keys.
        Logger::get().log("INFO", "DevKeys: " + path.string() + " not present (vanilla collab mode)");
        return false;
    }

    try {
        auto j = nlohmann::json::parse(fileData);
        // Tolerate missing fields — only flag loaded if the three the
        // host actually needs are present.
        memberPub = j.value("member_pub", "");
        appPub    = j.value("app_pub",    "");
        canvasId  = j.value("canvas_id",  "");
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO", std::string("DevKeys: parse failed: ") + e.what());
        return false;
    }

    if (memberPub.empty() || appPub.empty() || canvasId.empty()) {
        Logger::get().log("USERINFO",
            "DevKeys: " + path.string() + " missing required fields (member_pub, app_pub, canvas_id)");
        return false;
    }

    loaded = true;
    Logger::get().log("USERINFO", "DevKeys loaded: member=" + memberPub.substr(0, 8) +
                                  "... app=" + appPub.substr(0, 8) +
                                  "... canvas=" + canvasId);
    return true;
}
