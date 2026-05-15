// BIP-39 + SEP-0005 SLIP-0010 ed25519 derivation + ed25519 seed→pubkey.
//
// BIP-39 is wrapped around Trezor's vendored C implementation under
// trezor/. SLIP-0010 ed25519 hardened CKDpriv is implemented fresh here
// against the public SLIP-0010 spec (the algorithm Trezor's bip32.c uses
// for its ed25519 path — but written without dragging in the rest of
// bip32.c's curve baggage). ed25519 seed→pubkey delegates to the
// tweetnacl helper exposed in tweetnacl_seed_keypair.

#include "Stellar.hpp"
#include <cstring>

extern "C" {
    #include "trezor/bip39.h"
    #include "trezor/hmac.h"
    // Tweetnacl seed-keypair helper — added in the same workstream to
    // give us ed25519 scalar-base-mult without vendoring ed25519-donna.
    int crypto_sign_ed25519_tweet_seed_keypair(unsigned char* pk,
                                               unsigned char* sk,
                                               const unsigned char* seed);
    // OS-backed CSPRNG (deps/tweetnacl/randombytes.cpp).
    void randombytes(unsigned char* x, unsigned long long xlen);
}

namespace Stellar {

// ---- BIP-39 -------------------------------------------------------------

std::string generate_mnemonic_12() {
    // 12 words ↔ 128 bits of entropy ↔ 16 random bytes
    // (decision §8.2 of DISTRIBUTION-PHASE1.md).
    uint8_t entropy[16];
    randombytes(entropy, sizeof(entropy));

    const char* mnemo = mnemonic_from_data(entropy, sizeof(entropy));
    std::memset(entropy, 0, sizeof(entropy));
    if (!mnemo) return {};
    std::string out(mnemo);
    // Trezor's mnemo buffer is a static; explicit wipe + reset is
    // overkill but mirrors the upstream pattern.
    mnemonic_clear();
    return out;
}

bool mnemonic_valid(std::string_view mnemonic) {
    // mnemonic_check needs a null-terminated string. std::string_view
    // doesn't guarantee one, so copy.
    std::string m{mnemonic};
    return mnemonic_check(m.c_str()) != 0;
}

bool mnemonic_to_seed(std::string_view mnemonic,
                      std::string_view passphrase,
                      Bip39Seed& out) {
    std::string m{mnemonic};
    if (!mnemonic_check(m.c_str())) return false;
    std::string p{passphrase};
    ::mnemonic_to_seed(m.c_str(), p.c_str(), out.data(), nullptr);
    return true;
}

// ---- SLIP-0010 ed25519 hardened CKDpriv ---------------------------------

namespace {

// Big-endian serialization of a 32-bit hardened-bit-set index.
void be32(uint32_t v, uint8_t out[4]) {
    out[0] = static_cast<uint8_t>((v >> 24) & 0xff);
    out[1] = static_cast<uint8_t>((v >> 16) & 0xff);
    out[2] = static_cast<uint8_t>((v >>  8) & 0xff);
    out[3] = static_cast<uint8_t>( v        & 0xff);
}

// SLIP-0010 ed25519 master derivation:
//   I = HMAC-SHA512(key="ed25519 seed", data=master_seed)
//   k = I[0..32], c = I[32..64]
// Standard, matches Trezor bip32.c's hdnode_from_seed for ED25519_NAME.
void slip10_master(const uint8_t master_seed[64],
                   uint8_t out_k[32], uint8_t out_c[32]) {
    static const uint8_t KEY[] = "ed25519 seed";  // length 12, NOT NUL-terminated
    uint8_t I[64];
    hmac_sha512(KEY, static_cast<uint32_t>(sizeof(KEY) - 1), master_seed, 64, I);
    std::memcpy(out_k, I,        32);
    std::memcpy(out_c, I + 32,   32);
    std::memset(I, 0, sizeof(I));
}

// SLIP-0010 ed25519 hardened child derivation:
//   data = 0x00 || k_parent (32 bytes) || idx_ser (4 bytes BE)
//   I = HMAC-SHA512(key=c_parent, data=data)
//   k_child = I[0..32], c_child = I[32..64]
// idx MUST have the hardened bit set (idx >= 0x80000000); SLIP-0010 ed25519
// is *hardened-only* — non-hardened ed25519 derivation is undefined.
void slip10_ckd_hardened(const uint8_t k_in[32], const uint8_t c_in[32],
                          uint32_t idx,
                          uint8_t out_k[32], uint8_t out_c[32]) {
    uint8_t data[1 + 32 + 4];
    data[0] = 0x00;
    std::memcpy(data + 1, k_in, 32);
    be32(idx, data + 33);

    uint8_t I[64];
    hmac_sha512(c_in, 32, data, static_cast<uint32_t>(sizeof(data)), I);
    std::memcpy(out_k, I,        32);
    std::memcpy(out_c, I + 32,   32);
    std::memset(I, 0, sizeof(I));
    std::memset(data, 0, sizeof(data));
}

}  // namespace

void sep0005_derive(const Bip39Seed& master, uint32_t account, Seed& out) {
    // SEP-0005 Stellar path: m / 44' / 148' / <account>'
    constexpr uint32_t HARDENED = 0x80000000u;
    const uint32_t path[] = {
        HARDENED | 44u,
        HARDENED | 148u,
        HARDENED | account,
    };

    uint8_t k[32], c[32];
    slip10_master(master.data(), k, c);

    for (uint32_t idx : path) {
        uint8_t kn[32], cn[32];
        slip10_ckd_hardened(k, c, idx, kn, cn);
        std::memcpy(k, kn, 32);
        std::memcpy(c, cn, 32);
        std::memset(kn, 0, sizeof(kn));
        std::memset(cn, 0, sizeof(cn));
    }

    std::memcpy(out.data(), k, 32);
    std::memset(k, 0, sizeof(k));
    std::memset(c, 0, sizeof(c));
}

// ---- ed25519 seed → pubkey ---------------------------------------------

void seed_to_pubkey(const Seed& seed, PubKey& out) {
    // tweetnacl's seed-keypair fills sk[0..32] = seed, sk[32..64] = pk,
    // pk[0..32] = pk. We only want pk here; sk is a write-only scratch.
    unsigned char sk[64];
    crypto_sign_ed25519_tweet_seed_keypair(out.data(), sk, seed.data());
    std::memset(sk, 0, sizeof(sk));
}

}  // namespace Stellar
