#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class World;

// P0-C5/C6/C7 (DISTRIBUTION-PHASE0.md §5): Phase 0 access token verifier.
//
// Token format on the wire (URL-safe, ~160 bytes):
//
//   <base64url(64-byte ed25519 signature)> "." <base64url(json payload)>
//
// Payload (single-letter keys, sorted, compact JSON):
//
//   {
//     "a":   "GAB..." or hex,    artist member pubkey (signer)
//     "c":   "uuid",             canvas id
//     "e":   1761686400,         expires_at unix (omit = no expiry)
//     "i":   1730150400,         issued_at unix
//     "k":   "abcd...",          artist Inkternity app pubkey (hex)
//     "sub": "sha256(email)"     subscriber identity hash
//   }
//
// Host-side verification (the five checks; all must pass):
//
//   1. Signature: ed25519_verify(payload.a, signature, payload_bytes)
//   2. Identity:  payload.a == host's own member pubkey
//   3. App:       payload.k == host's own app pubkey
//   4. Canvas:    payload.c == host's open canvas_id
//   5. Expiry:    payload.e > now() if set
//
// The host self-verifies against its own loaded keys — no external
// lookup, no portal pubkey bundled in the binary.

namespace Subscription {

struct TokenPayload {
    std::string a;             // artist member pubkey
    std::string k;             // artist app pubkey
    std::string c;             // canvas id
    int64_t     i = 0;         // issued_at
    std::optional<int64_t> e;  // expires_at
    std::string sub;           // subscriber id hash (audit only)
};

enum class VerifyResult : uint8_t {
    OK,
    MALFORMED,           // wire format unparseable
    BAD_SIGNATURE,       // ed25519 verify failed
    IDENTITY_MISMATCH,   // payload.a != host member pubkey
    APP_MISMATCH,        // payload.k != host app pubkey
    CANVAS_MISMATCH,     // payload.c != host canvas id
    EXPIRED,             // payload.e <= now()
};

const char* verify_result_str(VerifyResult r);

// Pure parse + signature check. No binding checks — those need a
// World to compare against (see verify_token_for_host).
VerifyResult parse_and_check_signature(const std::string& token,
                                       TokenPayload& outPayload);

// Full host-side verification: parse, signature, identity, app,
// canvas, expiry. Returns VerifyResult::OK iff all five checks pass.
// `outPayload` is filled even on certain failures so callers can log.
VerifyResult verify_token_for_host(const std::string& token,
                                   const World& host,
                                   TokenPayload& outPayload);

}  // namespace Subscription
