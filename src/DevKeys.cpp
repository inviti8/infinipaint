#include "DevKeys.hpp"
#include <Helpers/Logger.hpp>
#include <Helpers/StringHelpers.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

extern "C" {
    #include "../deps/tweetnacl/tweetnacl.h"
}

namespace {

// 32 bytes -> 64 hex chars.
std::string to_hex(const unsigned char* bytes, size_t len) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(digits[(bytes[i] >> 4) & 0xf]);
        out.push_back(digits[bytes[i] & 0xf]);
    }
    return out;
}

}  // namespace

bool DevKeys::ensure_app_keypair(const std::filesystem::path& configPath) {
    const auto path = configPath / "inkternity_dev_keys.json";

    // Try to read whatever's there. Absent file or parse failure both
    // collapse to "empty starting state"; we'll write a fresh file.
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(read_file_to_string(path));
    } catch (...) {
        j = nlohmann::json::object();
    }

    // If both halves of the app keypair are already present, nothing to do.
    if (j.value("app_pub", "").size() == 64 &&
        j.value("app_secret", "").size() == 128) {
        return false;  // no change made
    }

    // Generate fresh ed25519 pair via tweetnacl. Output sizes:
    //   pk: 32 bytes (crypto_sign_PUBLICKEYBYTES)
    //   sk: 64 bytes (crypto_sign_SECRETKEYBYTES = seed || pk)
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(pk, sk);

    j["app_pub"]    = to_hex(pk, crypto_sign_PUBLICKEYBYTES);
    j["app_secret"] = to_hex(sk, crypto_sign_SECRETKEYBYTES);

    try {
        std::ofstream(path) << j.dump(2);
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO",
            std::string("DevKeys: could not write app keypair to ") + path.string() + ": " + e.what());
        return false;
    }

    Logger::get().log("USERINFO",
        "DevKeys: generated fresh app keypair at " + path.string() +
        " (pub=" + j["app_pub"].get<std::string>().substr(0, 8) + "...)");
    return true;
}

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
        memberPub = j.value("member_pub", "");
        appPub    = j.value("app_pub",    "");
        canvasId  = j.value("canvas_id",  "");
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO", std::string("DevKeys: parse failed: ") + e.what());
        return false;
    }

    // After P0-C1 partial: app_pub alone is the natural first-run
    // state (Inkternity generates it; member_pub + canvas_id come later
    // from dev_mint_token.py / future portal). Whatever's present is
    // exposed; consumers gate on the specific field they need.
    loaded = !appPub.empty();
    if (loaded) {
        const std::string memberPart = memberPub.empty() ? "(unset)" : memberPub.substr(0, 8) + "...";
        const std::string canvasPart = canvasId.empty()  ? "(unset)" : canvasId;
        Logger::get().log("USERINFO", "DevKeys loaded: app=" + appPub.substr(0, 8) +
                                      "... member=" + memberPart +
                                      " canvas=" + canvasPart);
    } else {
        Logger::get().log("USERINFO",
            "DevKeys: " + path.string() + " present but app_pub missing");
    }
    return loaded;
}
