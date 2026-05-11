#include "TokenVerifier.hpp"
#include "../World.hpp"

extern "C" {
    #include "../../deps/tweetnacl/tweetnacl.h"
}

#include <chrono>
#include <cstring>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace Subscription {

const char* verify_result_str(VerifyResult r) {
    switch (r) {
        case VerifyResult::OK:                return "ok";
        case VerifyResult::MALFORMED:         return "malformed";
        case VerifyResult::BAD_SIGNATURE:     return "bad signature";
        case VerifyResult::IDENTITY_MISMATCH: return "identity mismatch (payload.a)";
        case VerifyResult::APP_MISMATCH:      return "app mismatch (payload.k)";
        case VerifyResult::CANVAS_MISMATCH:   return "canvas mismatch (payload.c)";
        case VerifyResult::EXPIRED:           return "expired";
    }
    return "unknown";
}

namespace {

// URL-safe base64 decode (no padding required). Returns empty on
// malformed input.
std::vector<uint8_t> b64u_decode(std::string_view in) {
    static int8_t table[256];
    static bool table_inited = false;
    if (!table_inited) {
        for (int i = 0; i < 256; ++i) table[i] = -1;
        const char* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        for (int i = 0; i < 64; ++i) table[(uint8_t)alpha[i]] = (int8_t)i;
        table_inited = true;
    }
    std::vector<uint8_t> out;
    out.reserve((in.size() * 3) / 4);
    uint32_t buf = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=') break;
        int v = table[(uint8_t)c];
        if (v < 0) return {};  // invalid character
        buf = (buf << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)((buf >> bits) & 0xff));
        }
    }
    return out;
}

// Decode artist's pubkey (in payload.a) to the 32 raw bytes ed25519
// verify expects. Accepts two formats:
//   - Stellar G... (56-char base32 with 2-byte version + 4-byte CRC16)
//   - hex (64 hex chars)
// Returns empty vector if the format is unrecognized.
std::vector<uint8_t> decode_member_pubkey(const std::string& a) {
    // Hex form (64 chars): straightforward decode.
    if (a.size() == 64) {
        std::vector<uint8_t> out(32);
        for (size_t i = 0; i < 32; ++i) {
            auto hex_nibble = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex_nibble(a[i*2]);
            int lo = hex_nibble(a[i*2+1]);
            if (hi < 0 || lo < 0) return {};
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return out;
    }
    // Stellar G... form: base32-decoded payload is 35 bytes:
    //   1 byte  version (G = 0x30, prefix byte 6<<3 = 48)
    //   32 bytes pubkey
    //   2 bytes CRC16
    // We don't verify the CRC here — token signature already binds
    // the pubkey to the payload. Just extract the middle 32 bytes.
    if (a.size() == 56 && a[0] == 'G') {
        // Base32 decode (RFC 4648 alphabet, no padding).
        static int8_t b32[256];
        static bool b32_inited = false;
        if (!b32_inited) {
            for (int i = 0; i < 256; ++i) b32[i] = -1;
            const char* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
            for (int i = 0; i < 32; ++i) b32[(uint8_t)alpha[i]] = (int8_t)i;
            b32_inited = true;
        }
        std::vector<uint8_t> raw;
        raw.reserve(35);
        uint64_t buf = 0;
        int bits = 0;
        for (char c : a) {
            int v = b32[(uint8_t)c];
            if (v < 0) return {};
            buf = (buf << 5) | (uint64_t)v;
            bits += 5;
            if (bits >= 8) {
                bits -= 8;
                raw.push_back((uint8_t)((buf >> bits) & 0xff));
            }
        }
        if (raw.size() < 33) return {};  // 1 version + 32 pubkey
        return std::vector<uint8_t>(raw.begin() + 1, raw.begin() + 33);
    }
    return {};
}

bool ed25519_verify_raw(const uint8_t* pub32,
                        const uint8_t* sig64,
                        const uint8_t* msg, size_t msglen) {
    // tweetnacl's crypto_sign_open expects a "signed message" — the
    // 64-byte signature prepended to the message bytes — and writes
    // the original message to the output buffer. We don't need the
    // output (we already have the message), but the API needs the
    // buffer.
    std::vector<uint8_t> signedMsg(64 + msglen);
    std::memcpy(signedMsg.data(), sig64, 64);
    if (msglen) std::memcpy(signedMsg.data() + 64, msg, msglen);

    std::vector<uint8_t> outBuf(signedMsg.size());
    unsigned long long outLen = 0;
    int rc = crypto_sign_open(outBuf.data(), &outLen,
                              signedMsg.data(), signedMsg.size(),
                              pub32);
    return rc == 0;
}

}  // namespace

VerifyResult parse_and_check_signature(const std::string& token, TokenPayload& outPayload) {
    const auto dot = token.find('.');
    if (dot == std::string::npos) return VerifyResult::MALFORMED;

    auto sigBytes     = b64u_decode(std::string_view(token.data(), dot));
    auto payloadBytes = b64u_decode(std::string_view(token.data() + dot + 1,
                                                    token.size() - dot - 1));
    if (sigBytes.size() != 64 || payloadBytes.empty())
        return VerifyResult::MALFORMED;

    try {
        auto j = nlohmann::json::parse(payloadBytes.begin(), payloadBytes.end());
        outPayload.a = j.value("a", "");
        outPayload.k = j.value("k", "");
        outPayload.c = j.value("c", "");
        outPayload.i = j.value("i", int64_t{0});
        if (j.contains("e") && !j["e"].is_null())
            outPayload.e = j["e"].get<int64_t>();
        outPayload.sub = j.value("sub", "");
    } catch (const std::exception&) {
        return VerifyResult::MALFORMED;
    }

    if (outPayload.a.empty() || outPayload.k.empty() || outPayload.c.empty())
        return VerifyResult::MALFORMED;

    auto pub32 = decode_member_pubkey(outPayload.a);
    if (pub32.size() != 32) return VerifyResult::MALFORMED;

    if (!ed25519_verify_raw(pub32.data(), sigBytes.data(),
                            payloadBytes.data(), payloadBytes.size()))
        return VerifyResult::BAD_SIGNATURE;

    return VerifyResult::OK;
}

VerifyResult verify_token_for_host(const std::string& token,
                                   const World& host,
                                   TokenPayload& outPayload) {
    auto r = parse_and_check_signature(token, outPayload);
    if (r != VerifyResult::OK) return r;

    if (outPayload.a != host.artistMemberPubkey)
        return VerifyResult::IDENTITY_MISMATCH;
    if (outPayload.k != host.appPubkeyAtPublish)
        return VerifyResult::APP_MISMATCH;
    if (outPayload.c != host.canvasId)
        return VerifyResult::CANVAS_MISMATCH;
    if (outPayload.e.has_value()) {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (outPayload.e.value() < now) return VerifyResult::EXPIRED;
    }
    return VerifyResult::OK;
}

}  // namespace Subscription
