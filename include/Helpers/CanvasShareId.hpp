#pragma once
#include <cstdint>
#include <string>
#include <string_view>

// DISTRIBUTION-PHASE0.md §12.5 — derive an artist's per-canvas lobby share
// code deterministically from (app_secret, canvas_id), so SUBSCRIPTION
// hosting produces the same address every session and (with Phase 1
// app-keypair restoration shipping) across reinstalls.
//
// Both halves of the share code (global_id + local_id) use HMAC-SHA-512/256
// (manually constructed on top of tweetnacl's crypto_hash) with
// domain-separated labels so the two outputs are independent.
// Base32-lowercase encoding (RFC 4648 alphabet, `a-z2-7`) keeps the
// wire-format alphanumeric and URL-safe; the signaling server's routing key
// is opaque so this is shape-compatible with the random globalIDs the
// existing code produces.
//
// Phase 1 (DISTRIBUTION-PHASE1.md §3.2): the API takes the raw 32-byte
// ed25519 seed directly — DevKeys::app_seed_bytes() returns it regardless
// of whether the on-disk app_secret is in hex (legacy) or strkey form
// (post-migration). The encoding migration is a pure representation
// change: the same 32 seed bytes feed the HMAC either way, so every share
// code published before the migration resolves identically after.
namespace CanvasShareId {

    // Length of the two halves and the full share code, mirrored from
    // NetLibrary::{GLOBALID_LEN, LOCALID_LEN}. Kept here as named constants
    // so the encoder doesn't have to depend on NetLibrary.
    inline constexpr size_t GLOBAL_ID_LEN = 40;
    inline constexpr size_t LOCAL_ID_LEN  = 10;
    inline constexpr size_t SHARE_CODE_LEN = GLOBAL_ID_LEN + LOCAL_ID_LEN;

    // Returns a 40-char lowercase-base32 string suitable as the WSS path
    // component the signaling server uses for routing.
    std::string derive_global_id(const uint8_t app_seed[32],
                                  std::string_view canvas_id);

    // Returns a 10-char lowercase-base32 string suitable as the in-channel
    // serverLocalID.
    std::string derive_local_id(const uint8_t app_seed[32],
                                 std::string_view canvas_id);

    // Convenience: returns global_id + local_id (50 chars).
    std::string derive_share_code(const uint8_t app_seed[32],
                                   std::string_view canvas_id);

}  // namespace CanvasShareId
