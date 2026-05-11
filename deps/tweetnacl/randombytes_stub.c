/*
 * Inkternity uses tweetnacl ONLY for ed25519 signature verification
 * (crypto_sign_open). That code path never calls randombytes — only
 * keypair generation and signing do. We provide a stub so the linker
 * is satisfied; if it ever fires the program aborts so we'd notice
 * a regression immediately rather than getting predictable "random"
 * bytes.
 *
 * If Inkternity ever needs to sign or generate keys locally
 * (Phase 0.5+ when the app keypair generation lands), replace this
 * with a real implementation backed by SystemPrng / getentropy.
 */
#include <stdlib.h>

void randombytes(unsigned char *x, unsigned long long xlen) {
    (void)x; (void)xlen;
    abort();
}
