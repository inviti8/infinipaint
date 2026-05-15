/**
 * Copyright (c) 2013-2014 Tomas Dzetkulic
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT HMAC_SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __HMAC_H__
#define __HMAC_H__

/* ---- Inkternity downstream patch ----
 * Symbol-namespace prefix for Trezor's HMAC functions. libjuice (the
 * WebRTC ICE library we link as juice.lib) ships its own hmac.c that
 * also exports `hmac_sha256` — without this rename, the desktop link
 * fails with LNK2005. Prefixing here keeps every translation unit that
 * #includes hmac.h consistent (definitions in hmac.c, callers in
 * pbkdf2.c / bip39.c / our StellarMnemonic.cpp) without modifying the
 * vendored .c files.
 */
#define hmac_sha256          ink_trezor_hmac_sha256
#define hmac_sha256_Init     ink_trezor_hmac_sha256_Init
#define hmac_sha256_Update   ink_trezor_hmac_sha256_Update
#define hmac_sha256_Final    ink_trezor_hmac_sha256_Final
#define hmac_sha256_prepare  ink_trezor_hmac_sha256_prepare
#define hmac_sha512          ink_trezor_hmac_sha512
#define hmac_sha512_Init     ink_trezor_hmac_sha512_Init
#define hmac_sha512_Update   ink_trezor_hmac_sha512_Update
#define hmac_sha512_Final    ink_trezor_hmac_sha512_Final
#define hmac_sha512_prepare  ink_trezor_hmac_sha512_prepare
/* ---- end downstream patch ---- */

#include <stdint.h>
#include "sha2.h"

typedef struct _HMAC_SHA256_CTX {
  uint8_t o_key_pad[SHA256_BLOCK_LENGTH];
  SHA256_CTX ctx;
} HMAC_SHA256_CTX;

typedef struct _HMAC_SHA512_CTX {
  uint8_t o_key_pad[SHA512_BLOCK_LENGTH];
  SHA512_CTX ctx;
} HMAC_SHA512_CTX;

void hmac_sha256_Init(HMAC_SHA256_CTX *hctx, const uint8_t *key,
                      const uint32_t keylen);
void hmac_sha256_Update(HMAC_SHA256_CTX *hctx, const uint8_t *msg,
                        const uint32_t msglen);
void hmac_sha256_Final(HMAC_SHA256_CTX *hctx, uint8_t *hmac);
void hmac_sha256(const uint8_t *key, const uint32_t keylen, const uint8_t *msg,
                 const uint32_t msglen, uint8_t *hmac);
void hmac_sha256_prepare(const uint8_t *key, const uint32_t keylen,
                         uint32_t *opad_digest, uint32_t *ipad_digest);

void hmac_sha512_Init(HMAC_SHA512_CTX *hctx, const uint8_t *key,
                      const uint32_t keylen);
void hmac_sha512_Update(HMAC_SHA512_CTX *hctx, const uint8_t *msg,
                        const uint32_t msglen);
void hmac_sha512_Final(HMAC_SHA512_CTX *hctx, uint8_t *hmac);
void hmac_sha512(const uint8_t *key, const uint32_t keylen, const uint8_t *msg,
                 const uint32_t msglen, uint8_t *hmac);
void hmac_sha512_prepare(const uint8_t *key, const uint32_t keylen,
                         uint64_t *opad_digest, uint64_t *ipad_digest);

#endif
