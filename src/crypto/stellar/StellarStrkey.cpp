// Stellar strkey encode/decode.
//
// Implements the canonical strkey wire format documented in the Stellar
// SDK and stellar-core's src/crypto/StrKey.cpp (Apache-2.0). Written
// fresh against the spec; the format is small enough that vendoring
// stellar-core's code (which drags in their crypto headers, fmt, etc.)
// would cost more than it saves.
//
// Format (35 bytes encoded as 56 chars of base32):
//   [0]      version byte
//   [1..33)  32-byte payload (ed25519 pubkey or seed)
//   [33..35) 2-byte CRC16-XMODEM little-endian, computed over [0..33)
//
// Version bytes:
//   Account ID (G...) : 6 << 3 = 0x30
//   Seed       (S...) : 18 << 3 = 0x90
//
// Base32 alphabet: RFC 4648 *uppercase* (Stellar's convention, distinct
// from the lowercased alphabet CanvasShareId uses for share codes).
//
// CRC16-XMODEM: poly 0x1021, init 0x0000, no XOR-out, *no* bit reflection
// (this is the same variant stellar-core uses; libraries also call it
// "CRC-16/XMODEM" or "CRC-CCITT/0x0000").

#include "Stellar.hpp"
#include <cstring>

namespace Stellar {

namespace {

constexpr char BASE32_UPPER[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

constexpr uint8_t VERSION_ACCOUNT_ID = 6  << 3;  // 0x30 → 'G' first char after b32 encoding
constexpr uint8_t VERSION_SEED       = 18 << 3;  // 0x90 → 'S' first char after b32 encoding

constexpr size_t RAW_BYTES = 1 + 32 + 2;  // version + payload + CRC = 35

// XMODEM CRC-16 — poly 0x1021, init 0x0000, no XOR-out, no reflection.
// Operates on every byte from version + payload (33 bytes for us).
uint16_t crc16_xmodem(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

// Encode `raw_len` bytes (RAW_BYTES = 35 for strkey) into base32 RFC 4648
// uppercase. Output is exactly ceil(raw_len * 8 / 5) = 56 chars for
// 35-byte input. No padding emitted; for 35-byte input the encoding ends
// on a byte boundary so the standard "==" tail isn't needed.
std::string base32_encode(const uint8_t* raw, size_t raw_len) {
    std::string out;
    out.reserve((raw_len * 8 + 4) / 5);

    uint32_t buffer = 0;
    int bits = 0;
    for (size_t i = 0; i < raw_len; ++i) {
        buffer = (buffer << 8) | raw[i];
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out.push_back(BASE32_UPPER[(buffer >> bits) & 0x1f]);
        }
    }
    if (bits > 0) {
        // Pad remaining bits with zeros up to a 5-bit boundary.
        out.push_back(BASE32_UPPER[(buffer << (5 - bits)) & 0x1f]);
    }
    return out;
}

// Decode `in_len` chars of base32 uppercase. Strict — rejects lowercase,
// rejects any char outside the alphabet. Writes up to `out_max` bytes to
// `out`. Returns the number of bytes written, or 0 on any error.
// For strkey we expect 56 input chars → 35 output bytes.
size_t base32_decode_strict(std::string_view in, uint8_t* out, size_t out_max) {
    uint32_t buffer = 0;
    int bits = 0;
    size_t written = 0;
    for (char c : in) {
        int v;
        if (c >= 'A' && c <= 'Z')      v = c - 'A';
        else if (c >= '2' && c <= '7') v = 26 + (c - '2');
        else return 0;  // invalid char (incl lowercase, padding, whitespace)

        buffer = (buffer << 5) | static_cast<uint32_t>(v);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            if (written >= out_max) return 0;
            out[written++] = static_cast<uint8_t>((buffer >> bits) & 0xff);
        }
    }
    return written;
}

// Encode (version || payload32) + CRC16 → 56-char strkey.
std::string encode_with_version(uint8_t version, const uint8_t payload[32]) {
    uint8_t raw[RAW_BYTES];
    raw[0] = version;
    std::memcpy(raw + 1, payload, 32);
    const uint16_t crc = crc16_xmodem(raw, 1 + 32);
    raw[33] = static_cast<uint8_t>(crc & 0xff);          // little-endian
    raw[34] = static_cast<uint8_t>((crc >> 8) & 0xff);
    return base32_encode(raw, RAW_BYTES);
}

// Decode a 56-char strkey. Returns true and writes 32-byte payload to
// `out` iff: length is 56, decodes to 35 bytes, version byte matches
// `expected_version`, and CRC-16 over [0..33) matches the trailing
// little-endian 2 bytes.
bool decode_with_version(std::string_view strkey, uint8_t expected_version, uint8_t out[32]) {
    if (strkey.size() != STRKEY_CHARS) return false;
    uint8_t raw[RAW_BYTES];
    if (base32_decode_strict(strkey, raw, RAW_BYTES) != RAW_BYTES) return false;
    if (raw[0] != expected_version) return false;
    const uint16_t want = crc16_xmodem(raw, 1 + 32);
    const uint16_t got  = static_cast<uint16_t>(raw[33]) |
                          (static_cast<uint16_t>(raw[34]) << 8);
    if (want != got) return false;
    std::memcpy(out, raw + 1, 32);
    return true;
}

}  // namespace

std::string encode_pubkey(const PubKey& pub) {
    return encode_with_version(VERSION_ACCOUNT_ID, pub.data());
}

std::string encode_seed(const Seed& seed) {
    return encode_with_version(VERSION_SEED, seed.data());
}

bool decode_pubkey(std::string_view strkey, PubKey& out) {
    return decode_with_version(strkey, VERSION_ACCOUNT_ID, out.data());
}

bool decode_seed(std::string_view strkey, Seed& out) {
    return decode_with_version(strkey, VERSION_SEED, out.data());
}

}  // namespace Stellar
