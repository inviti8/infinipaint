#pragma once
// DISTRIBUTION-PHASE1 §3 — Stellar-keypair app identity (self-custodial).
//
// Public C++ API for the four primitives Inkternity needs to encode its
// app keypair as a Stellar account:
//   - BIP-39 mnemonic generate / check / seed-derive (Trezor MIT C
//     sources under trezor/, wrapped here)
//   - SLIP-0010 ed25519 hardened CKDpriv along the SEP-0005 Stellar path
//     m/44'/148'/account' (implemented fresh against the public SLIP-0010
//     and SEP-0005 specs; algorithm cross-checked against Trezor's
//     bip32.c ed25519 path)
//   - Stellar strkey base32+CRC16 encode/decode for G... pubkey and
//     S... seed (implemented fresh against the public stellar-core
//     StrKey.cpp reference)
//   - ed25519 seed → pubkey scalar-base-mult (delegated to the in-tree
//     tweetnacl seed-keypair helper)
//
// None of these touch the network. The Horizon balance probe in
// FileSelectScreen is a separate, optional, on-demand HTTP GET.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace Stellar {

// ---- Sizes ---------------------------------------------------------------

inline constexpr size_t SEED_BYTES        = 32;   // ed25519 seed (== Stellar account secret bytes)
inline constexpr size_t PUBKEY_BYTES      = 32;   // ed25519 public key
inline constexpr size_t BIP39_SEED_BYTES  = 64;   // PBKDF2 output, fed into SLIP-0010 master
inline constexpr size_t STRKEY_CHARS      = 56;   // G... / S... base32 length

using Seed   = std::array<uint8_t, SEED_BYTES>;
using PubKey = std::array<uint8_t, PUBKEY_BYTES>;
using Bip39Seed = std::array<uint8_t, BIP39_SEED_BYTES>;

// ---- BIP-39 --------------------------------------------------------------

// Generate a fresh 12-word English mnemonic from 128 bits of OS entropy.
// Returns the canonical lowercase space-separated mnemonic string.
// Empty string on RNG failure (does not normally happen).
std::string generate_mnemonic_12();

// True if `mnemonic` is a syntactically valid + checksum-correct BIP-39
// mnemonic in the English wordlist with 12/18/24 words.
bool mnemonic_valid(std::string_view mnemonic);

// PBKDF2-HMAC-SHA512 over (mnemonic, "mnemonic" || passphrase, 2048
// rounds) — the canonical BIP-39 seed derivation. Writes 64 bytes
// into `out`. Returns false if `mnemonic` fails validation.
bool mnemonic_to_seed(std::string_view mnemonic,
                       std::string_view passphrase,
                       Bip39Seed& out);

// ---- SEP-0005 SLIP-0010 ed25519 hardened derivation ----------------------

// Derives the Stellar account seed at path m/44'/148'/<account>' from a
// 64-byte BIP-39 master seed via SLIP-0010 ed25519 hardened CKDpriv.
// `account` is the SEP-0005 account index (0 = primary).
// Always succeeds for any 64-byte input + any non-negative account index.
void sep0005_derive(const Bip39Seed& master, uint32_t account, Seed& out);

// ---- ed25519 seed → pubkey ----------------------------------------------

// Computes the ed25519 public key for a 32-byte private seed via
// scalar-base-mult. Wraps the tweetnacl seed-keypair helper.
void seed_to_pubkey(const Seed& seed, PubKey& out);

// ---- Stellar strkey encode/decode ---------------------------------------

// Encode a 32-byte ed25519 pubkey as a 56-char strkey starting with 'G'.
// Always returns a 56-char string.
std::string encode_pubkey(const PubKey& pub);

// Encode a 32-byte ed25519 seed as a 56-char strkey starting with 'S'.
// Always returns a 56-char string.
std::string encode_seed(const Seed& seed);

// Decode a G... strkey to 32 bytes. Returns false on any of: wrong
// length, wrong version byte, bad base32 char, bad CRC.
bool decode_pubkey(std::string_view strkey, PubKey& out);

// Decode an S... strkey to 32 bytes. Same failure modes as decode_pubkey.
bool decode_seed(std::string_view strkey, Seed& out);

// Quick format probe — returns true if `s` is exactly STRKEY_CHARS long
// and starts with 'G'/'S'. Does NOT validate checksum; callers that need
// strict validation use decode_* directly.
inline bool looks_like_pubkey_strkey(std::string_view s) {
    return s.size() == STRKEY_CHARS && (s.front() == 'G');
}
inline bool looks_like_seed_strkey(std::string_view s) {
    return s.size() == STRKEY_CHARS && (s.front() == 'S');
}

}  // namespace Stellar
