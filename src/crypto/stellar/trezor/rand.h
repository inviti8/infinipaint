// Local shim — Trezor's upstream bip39.c #includes "rand.h" but never
// actually calls any random_* function from it (entropy is passed in
// by the caller via mnemonic_from_data). Rather than dropping the
// stale include in the vendored bip39.c, we provide this empty
// header so the file builds verbatim. If a future Trezor sync ever
// makes bip39 actually rely on rand.h, this shim will fail to compile
// and we'll wire it to our existing OS RNG (deps/tweetnacl/randombytes.cpp).
#ifndef INKTERNITY_TREZOR_RAND_SHIM_H
#define INKTERNITY_TREZOR_RAND_SHIM_H
#endif
