/*
 * randombytes for tweetnacl, backed by std::random_device.
 *
 * tweetnacl needs randombytes() defined externally — its own
 * crypto_sign_keypair() and crypto_sign() paths call it for seed
 * generation. (Verification — crypto_sign_open — does not.)
 *
 * This implementation uses std::random_device, which on every
 * supported platform (Windows, macOS, Linux) is backed by the OS
 * cryptographic RNG: BCryptGenRandom / SecRandomCopyBytes /
 * getrandom() respectively. Quality is sufficient for ed25519
 * keypair generation in Phase 0.
 *
 * Linked as C++ but exposed as extern "C" so tweetnacl's C code
 * resolves the symbol normally.
 */
#include <random>

extern "C" void randombytes(unsigned char *x, unsigned long long xlen) {
    static thread_local std::random_device rd;
    // std::random_device produces uint32_t per call on most
    // implementations; pull a chunk at a time and copy out the
    // bytes we need.
    while (xlen > 0) {
        std::uint32_t v = rd();
        unsigned long long take = (xlen < sizeof(v)) ? xlen : sizeof(v);
        for (unsigned long long i = 0; i < take; ++i) {
            x[i] = static_cast<unsigned char>((v >> (i * 8)) & 0xff);
        }
        x   += take;
        xlen -= take;
    }
}
