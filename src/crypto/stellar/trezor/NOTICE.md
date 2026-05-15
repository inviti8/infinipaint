# Vendored sources — Trezor crypto

The files in this directory are vendored verbatim from upstream
`trezor/trezor-firmware` at `crypto/` and carry the original MIT
copyright headers in-file.

- Upstream: https://github.com/trezor/trezor-firmware/tree/main/crypto
- License: MIT (Pavol Rusnak, Tomas Dzetkulic, et al. — see each file)
- Sync date: 2026-05-14

Files retained:
- bip39.{c,h}            BIP-39 mnemonic encode/decode + PBKDF2 seed derivation
- bip39_english.c        2048-word BIP-39 English wordlist
- hmac.{c,h}             HMAC-SHA-256, HMAC-SHA-512
- pbkdf2.{c,h}           PBKDF2-HMAC-SHA-256, PBKDF2-HMAC-SHA-512
- sha2.{c,h}             SHA-256, SHA-512
- memzero.{c,h}          Compiler-fence-protected zero-on-clear
- byte_order.h           Endian helpers used by sha2.c
- options.h              Trezor's compile-time configuration knobs

Files NOT used:
- bip32.{c,h}            Drags in ECDSA / secp256k1 / nist256p1 / ed25519-donna /
                         AES / base58 / bignum / cardano / NEM / sha3, all
                         unrelated to our ed25519-only need. SLIP-0010
                         ed25519 derivation is implemented fresh in
                         ../slip10_ed25519.{hpp,cpp}, using the same
                         algorithm Trezor's bip32.c uses (HMAC-SHA-512
                         with the SLIP-0010 input format), but without
                         the curve baggage.

Local additions (not from Trezor):
- rand.h                 Empty shim. bip39.c #includes "rand.h" but its
                         compile-active code never calls any random_*
                         function (entropy comes from the caller). The
                         shim keeps the vendored bip39.c byte-identical
                         to upstream rather than carrying a downstream
                         patch.

Compile-time overrides applied via CMake (see CMakeLists.txt):
- USE_BIP39_CACHE=0      Disables the in-memory mnemonic cache in
                         bip39.c. We don't want plaintext mnemonics
                         lingering in a desktop process across the
                         artist's session.
