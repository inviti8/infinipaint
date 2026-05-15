#include "DevKeys.hpp"
#include "../src/crypto/stellar/Stellar.hpp"
#include <Helpers/Logger.hpp>
#include <Helpers/StringHelpers.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

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

int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// Decode the first 32 bytes (= 64 hex chars) of an arbitrarily long
// hex string. Returns false on any non-hex char or short input.
bool hex_decode_32(const std::string& hex, uint8_t out[32]) {
    if (hex.size() < 64) return false;
    for (size_t i = 0; i < 32; ++i) {
        const int hi = hex_nibble(hex[2 * i]);
        const int lo = hex_nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

// Detection: is `s` a Stellar G... pubkey strkey?
bool is_strkey_pub(const std::string& s) {
    return Stellar::looks_like_pubkey_strkey(s);
}
// Detection: is `s` a Stellar S... seed strkey?
bool is_strkey_seed(const std::string& s) {
    return Stellar::looks_like_seed_strkey(s);
}
// Detection: is `s` legacy hex (only relevant lengths shown — 64 for pub,
// 128 for secret).
bool is_hex_pub(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s) if (hex_nibble(c) < 0) return false;
    return true;
}
bool is_hex_secret(const std::string& s) {
    if (s.size() != 128) return false;
    for (char c : s) if (hex_nibble(c) < 0) return false;
    return true;
}

// Generate fresh app keypair via BIP-39 mnemonic → SEP-0005 derivation.
// Writes G..., S..., and the mnemonic into `j`.
void generate_fresh_keypair_into(nlohmann::json& j) {
    const std::string mnemo = Stellar::generate_mnemonic_12();
    Stellar::Bip39Seed master{};
    Stellar::mnemonic_to_seed(mnemo, "", master);
    Stellar::Seed seed{};
    Stellar::sep0005_derive(master, /*account=*/0, seed);
    Stellar::PubKey pub{};
    Stellar::seed_to_pubkey(seed, pub);

    j["app_pub"]      = Stellar::encode_pubkey(pub);
    j["app_secret"]   = Stellar::encode_seed(seed);
    j["app_mnemonic"] = mnemo;

    // Best-effort wipe of stack scratch — these arrays go out of scope
    // immediately, but explicit clear matches the in-tree pattern.
    std::fill(seed.begin(), seed.end(), uint8_t{0});
    std::fill(master.begin(), master.end(), uint8_t{0});
}

// Migrate hex-format keys to strkey, preserving the exact 32-byte seed
// (so every share code derived from those bytes via DISTRIBUTION-PHASE0.md
// §12.5 keeps resolving). Returns true iff `j` was mutated.
bool migrate_hex_to_strkey(nlohmann::json& j) {
    const std::string hexSecret = j.value("app_secret", "");
    if (!is_hex_secret(hexSecret)) return false;

    uint8_t seedBytes[32];
    if (!hex_decode_32(hexSecret, seedBytes)) return false;

    Stellar::Seed seed{};
    std::copy(seedBytes, seedBytes + 32, seed.begin());
    Stellar::PubKey pub{};
    Stellar::seed_to_pubkey(seed, pub);

    j["app_pub"]    = Stellar::encode_pubkey(pub);
    j["app_secret"] = Stellar::encode_seed(seed);
    // app_mnemonic intentionally NOT set — these legacy random seeds
    // had no mnemonic backing them. Export App Key UI keys off
    // mnemonic.empty() to know whether to surface the mnemonic block.

    std::fill(seed.begin(), seed.end(), uint8_t{0});
    std::memset(seedBytes, 0, sizeof(seedBytes));
    return true;
}

}  // namespace

bool DevKeys::ensure_app_keypair(const std::filesystem::path& configPath) {
    const auto path = configPath / "inkternity_dev_keys.json";

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(read_file_to_string(path));
    } catch (...) {
        j = nlohmann::json::object();
    }

    const std::string curPub    = j.value("app_pub",    "");
    const std::string curSecret = j.value("app_secret", "");

    enum class Action { NoOp, Migrate, Generate };
    Action action;
    if (is_strkey_pub(curPub) && is_strkey_seed(curSecret)) {
        action = Action::NoOp;
    } else if (is_hex_pub(curPub) && is_hex_secret(curSecret)) {
        action = Action::Migrate;
    } else {
        action = Action::Generate;
    }

    if (action == Action::NoOp) return false;

    if (action == Action::Migrate) {
        if (!migrate_hex_to_strkey(j)) {
            // Hex secret looked plausible by length but failed to decode.
            // Fall through to generate-fresh rather than leave a half-state.
            action = Action::Generate;
        }
    }
    if (action == Action::Generate) {
        generate_fresh_keypair_into(j);
    }

    try {
        std::ofstream(path) << j.dump(2);
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO",
            std::string("DevKeys: could not write app keypair to ") + path.string() + ": " + e.what());
        return false;
    }

    const std::string note = (action == Action::Migrate)
        ? "DevKeys: migrated app keypair hex → Stellar strkey at "
        : "DevKeys: generated fresh Stellar app keypair at ";
    Logger::get().log("USERINFO",
        note + path.string() +
        " (pub=" + j.value("app_pub", "").substr(0, 8) + "...)");
    return true;
}

namespace {

// Trim leading + trailing whitespace. The user pastes from a clipboard
// which often carries stray spaces/newlines.
std::string trim(const std::string& s) {
    auto isWs = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    size_t lo = 0, hi = s.size();
    while (lo < hi && isWs(s[lo])) ++lo;
    while (hi > lo && isWs(s[hi - 1])) --hi;
    return s.substr(lo, hi - lo);
}

// True if `s` looks like a single S... strkey (no internal whitespace,
// 56 chars, leading 'S'). Validation of the actual checksum happens
// downstream in Stellar::decode_seed.
bool is_single_token_strkey_seed(const std::string& s) {
    if (!Stellar::looks_like_seed_strkey(s)) return false;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return false;
    }
    return true;
}

}  // namespace

bool DevKeys::restore_from_input(const std::string& rawInput,
                                  const std::filesystem::path& configPath) {
    const std::string input = trim(rawInput);
    if (input.empty()) return false;

    // Auto-detect: a 56-char single-token S... is the strkey path; anything
    // with whitespace (or different length) is treated as a mnemonic.
    Stellar::Seed seed{};
    std::string newMnemonic;  // empty unless we restore from a mnemonic

    if (is_single_token_strkey_seed(input)) {
        if (!Stellar::decode_seed(input, seed)) return false;
        // Restoring from raw S... — no mnemonic to record. The artist is
        // expected to have the S... saved elsewhere; we deliberately do
        // NOT generate a fresh mnemonic for them (would be wrong — there's
        // no mnemonic that derives this specific seed via SEP-0005).
    } else {
        if (!Stellar::mnemonic_valid(input)) return false;
        Stellar::Bip39Seed master{};
        if (!Stellar::mnemonic_to_seed(input, "", master)) return false;
        Stellar::sep0005_derive(master, /*account=*/0, seed);
        newMnemonic = input;  // canonicalize to whatever the user pasted
        std::fill(master.begin(), master.end(), uint8_t{0});
    }

    Stellar::PubKey pub{};
    Stellar::seed_to_pubkey(seed, pub);
    const std::string newPub    = Stellar::encode_pubkey(pub);
    const std::string newSecret = Stellar::encode_seed(seed);
    std::fill(seed.begin(), seed.end(), uint8_t{0});

    // Read whatever's there so we preserve member_pub / member_secret /
    // canvas_id (these aren't ours — they belong to dev_mint_token.py /
    // future portal-issued credentials). Only the app_* triple changes.
    const auto path = configPath / "inkternity_dev_keys.json";
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(read_file_to_string(path));
    } catch (...) {
        j = nlohmann::json::object();
    }
    j["app_pub"]      = newPub;
    j["app_secret"]   = newSecret;
    if (newMnemonic.empty()) {
        j.erase("app_mnemonic");
    } else {
        j["app_mnemonic"] = newMnemonic;
    }

    try {
        std::ofstream(path) << j.dump(2);
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO",
            std::string("DevKeys: restore failed to write file: ") + e.what());
        return false;
    }

    Logger::get().log("USERINFO",
        "DevKeys: restored app keypair via " +
        std::string(newMnemonic.empty() ? "S... strkey" : "mnemonic") +
        " — pub=" + newPub.substr(0, 8) + "...");

    // Reload in-memory state from the now-canonical file.
    return load(configPath);
}

bool DevKeys::load(const std::filesystem::path& configPath) {
    const auto path = configPath / "inkternity_dev_keys.json";
    std::string fileData;
    try {
        fileData = read_file_to_string(path);
    } catch (...) {
        Logger::get().log("INFO", "DevKeys: " + path.string() + " not present (vanilla collab mode)");
        return false;
    }

    try {
        auto j = nlohmann::json::parse(fileData);
        memberPub    = j.value("member_pub",    "");
        appPub       = j.value("app_pub",       "");
        appSecret    = j.value("app_secret",    "");
        appMnemonic  = j.value("app_mnemonic",  "");
        canvasId     = j.value("canvas_id",     "");
    } catch (const std::exception& e) {
        Logger::get().log("USERINFO", std::string("DevKeys: parse failed: ") + e.what());
        return false;
    }

    // Decode the seed bytes from whatever encoding `app_secret` happens
    // to be in. Strkey is the canonical post-Phase-1 form; hex is the
    // legacy form. After ensure_app_keypair runs at startup the file
    // should always be strkey, but load() stays defensive.
    appSeedBytes.fill(0);
    if (is_strkey_seed(appSecret)) {
        Stellar::Seed seed{};
        if (Stellar::decode_seed(appSecret, seed)) {
            std::copy(seed.begin(), seed.end(), appSeedBytes.begin());
            std::fill(seed.begin(), seed.end(), uint8_t{0});
        }
    } else if (is_hex_secret(appSecret)) {
        hex_decode_32(appSecret, appSeedBytes.data());
    }

    loaded = !appPub.empty();
    if (loaded) {
        const std::string memberPart = memberPub.empty() ? "(unset)" : memberPub.substr(0, 8) + "...";
        const std::string canvasPart = canvasId.empty()  ? "(unset)" : canvasId;
        const std::string mnemoNote  = appMnemonic.empty() ? " (no mnemonic — migrated from hex)" : "";
        Logger::get().log("USERINFO", "DevKeys loaded: app=" + appPub.substr(0, 8) +
                                      "... member=" + memberPart +
                                      " canvas=" + canvasPart + mnemoNote);
    } else {
        Logger::get().log("USERINFO",
            "DevKeys: " + path.string() + " present but app_pub missing");
    }
    return loaded;
}
