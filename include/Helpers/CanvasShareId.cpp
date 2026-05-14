#include "CanvasShareId.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
    #include "../../deps/tweetnacl/tweetnacl.h"
}

namespace CanvasShareId {

namespace {

constexpr const char* GLOBAL_ID_LABEL = "inkternity:canvas-globalid:v1|";
constexpr const char* LOCAL_ID_LABEL  = "inkternity:canvas-localid:v1|";

// RFC 4648 base32 alphabet, lowercased. 32 symbols, all alphanumeric, all
// URL-safe. We don't emit padding because the consumer truncates anyway.
constexpr const char BASE32_ALPHABET[] = "abcdefghijklmnopqrstuvwxyz234567";

// SHA-512 parameters. tweetnacl ships crypto_hash (SHA-512, 64-byte output)
// but its stripped tweetnacl.c does NOT include crypto_auth_hmacsha512256,
// so we build HMAC-SHA-512 from the hash primitive ourselves.
constexpr size_t SHA512_BLOCK = 128;
constexpr size_t SHA512_OUT   = 64;

// Output of HMAC-SHA-512/256 (= HMAC-SHA-512 truncated to 32 bytes).
// 256 bits gives 51+ base32 chars — comfortably enough for the 40-char
// global_id we truncate to and the 10-char local_id.
constexpr size_t HMAC_OUT = 32;

// HMAC key derived from the ed25519 secret seed. The seed is 32 bytes,
// which is well under the SHA-512 block size, so we zero-pad to the block.
constexpr size_t KEY_LEN = 32;

int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// Decodes the first `expected_bytes` of hex from `hex` into `out`. Returns
// false if `hex` is too short or contains a non-hex character. Used for
// the app_secret seed extraction (first 32 bytes = 64 hex chars).
bool hex_decode(std::string_view hex, size_t expected_bytes, unsigned char* out) {
    if (hex.size() < expected_bytes * 2) return false;
    for (size_t i = 0; i < expected_bytes; ++i) {
        const int hi = hex_nibble(hex[2 * i]);
        const int lo = hex_nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return true;
}

// SHA-512 wrapper around tweetnacl's crypto_hash. Returns out[0..63].
void sha512(unsigned char out[SHA512_OUT], const unsigned char* msg, size_t msg_len) {
    crypto_hash(out, msg, static_cast<unsigned long long>(msg_len));
}

// HMAC-SHA-512 truncated to 32 bytes (HMAC-SHA-512/256, RFC 6234 variant
// used in tweetnacl's full crypto_auth). Standard textbook construction:
//   K' = key zero-padded to 128 bytes (or hashed first if longer)
//   inner = SHA-512((K' XOR 0x36..) || msg)
//   outer = SHA-512((K' XOR 0x5C..) || inner)
//   tag = outer[0..31]
//
// We never call this with key longer than 32 bytes so the "hash long key"
// branch is omitted.
std::array<unsigned char, HMAC_OUT> hmac_sha512_256(
    const unsigned char key[KEY_LEN],
    std::string_view label,
    std::string_view canvas_id)
{
    unsigned char k_ipad[SHA512_BLOCK];
    unsigned char k_opad[SHA512_BLOCK];
    std::memset(k_ipad, 0, SHA512_BLOCK);
    std::memset(k_opad, 0, SHA512_BLOCK);
    std::memcpy(k_ipad, key, KEY_LEN);
    std::memcpy(k_opad, key, KEY_LEN);
    for (size_t i = 0; i < SHA512_BLOCK; ++i) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5C;
    }

    // inner = SHA-512(k_ipad || label || canvas_id)
    std::vector<unsigned char> inner_buf;
    inner_buf.reserve(SHA512_BLOCK + label.size() + canvas_id.size());
    inner_buf.insert(inner_buf.end(), k_ipad, k_ipad + SHA512_BLOCK);
    inner_buf.insert(inner_buf.end(),
        reinterpret_cast<const unsigned char*>(label.data()),
        reinterpret_cast<const unsigned char*>(label.data()) + label.size());
    inner_buf.insert(inner_buf.end(),
        reinterpret_cast<const unsigned char*>(canvas_id.data()),
        reinterpret_cast<const unsigned char*>(canvas_id.data()) + canvas_id.size());

    unsigned char inner_hash[SHA512_OUT];
    sha512(inner_hash, inner_buf.data(), inner_buf.size());

    // outer = SHA-512(k_opad || inner_hash)
    unsigned char outer_buf[SHA512_BLOCK + SHA512_OUT];
    std::memcpy(outer_buf, k_opad, SHA512_BLOCK);
    std::memcpy(outer_buf + SHA512_BLOCK, inner_hash, SHA512_OUT);
    unsigned char outer_hash[SHA512_OUT];
    sha512(outer_hash, outer_buf, SHA512_BLOCK + SHA512_OUT);

    std::array<unsigned char, HMAC_OUT> tag{};
    std::memcpy(tag.data(), outer_hash, HMAC_OUT);
    return tag;
}

// Encode `tag` (32 bytes = 256 bits) into base32-lowercase, truncated to
// `out_len` chars. out_len must be <= 51 (the full encoding length).
std::string base32_truncated(const std::array<unsigned char, HMAC_OUT>& tag,
                              size_t out_len) {
    std::string out;
    out.reserve(out_len);

    // Bit-accumulator: shift in MSB-first, emit a char per 5 bits.
    uint32_t buffer = 0;
    int bits_in_buffer = 0;
    size_t emitted = 0;
    for (size_t i = 0; i < tag.size() && emitted < out_len; ++i) {
        buffer = (buffer << 8) | tag[i];
        bits_in_buffer += 8;
        while (bits_in_buffer >= 5 && emitted < out_len) {
            bits_in_buffer -= 5;
            const uint32_t idx = (buffer >> bits_in_buffer) & 0x1f;
            out.push_back(BASE32_ALPHABET[idx]);
            ++emitted;
        }
    }
    return out;
}

std::string derive(std::string_view app_secret_hex,
                    std::string_view canvas_id,
                    std::string_view label,
                    size_t out_len) {
    unsigned char key[KEY_LEN];
    if (!hex_decode(app_secret_hex, KEY_LEN, key)) return {};
    const auto tag = hmac_sha512_256(key, label, canvas_id);
    return base32_truncated(tag, out_len);
}

}  // namespace

std::string derive_global_id(std::string_view app_secret_hex,
                              std::string_view canvas_id) {
    return derive(app_secret_hex, canvas_id, GLOBAL_ID_LABEL, GLOBAL_ID_LEN);
}

std::string derive_local_id(std::string_view app_secret_hex,
                             std::string_view canvas_id) {
    return derive(app_secret_hex, canvas_id, LOCAL_ID_LABEL, LOCAL_ID_LEN);
}

std::string derive_share_code(std::string_view app_secret_hex,
                               std::string_view canvas_id) {
    auto g = derive_global_id(app_secret_hex, canvas_id);
    auto l = derive_local_id(app_secret_hex, canvas_id);
    if (g.empty() || l.empty()) return {};
    return g + l;
}

}  // namespace CanvasShareId
