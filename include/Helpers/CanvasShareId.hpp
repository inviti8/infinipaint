#pragma once
#include <string>
#include <string_view>

// DISTRIBUTION-PHASE0.md §12.5 — derive an artist's per-canvas lobby share
// code deterministically from (app_secret, canvas_id), so SUBSCRIPTION
// hosting produces the same address every session and (once Phase 1
// app-keypair restoration ships) across reinstalls.
//
// Both halves of the share code (global_id + local_id) use HMAC-SHA-512/256
// (crypto_auth_hmacsha512256 in tweetnacl) with domain-separated labels so
// the two outputs are independent. Base32-lowercase encoding (RFC 4648
// alphabet, `a-z2-7`) keeps the wire-format alphanumeric and URL-safe;
// the signaling server's routing key is opaque so this is shape-compatible
// with the random globalIDs the existing code produces.
//
// app_secret_hex must be the hex-encoded ed25519 secret key from
// DevKeys::app_secret() (128 hex chars = 64 bytes: seed || pubkey). Only the
// first 32 bytes (the seed) are fed to the HMAC; the trailing pubkey half
// adds no entropy.
namespace CanvasShareId {

    // Length of the two halves and the full share code, mirrored from
    // NetLibrary::{GLOBALID_LEN, LOCALID_LEN}. Kept here as named constants
    // so the encoder doesn't have to depend on NetLibrary.
    inline constexpr size_t GLOBAL_ID_LEN = 40;
    inline constexpr size_t LOCAL_ID_LEN  = 10;
    inline constexpr size_t SHARE_CODE_LEN = GLOBAL_ID_LEN + LOCAL_ID_LEN;

    // Returns a 40-char lowercase-base32 string suitable as the WSS path
    // component the signaling server uses for routing. Empty string on
    // invalid input (e.g. malformed app_secret hex).
    std::string derive_global_id(std::string_view app_secret_hex,
                                  std::string_view canvas_id);

    // Returns a 10-char lowercase-base32 string suitable as the in-channel
    // serverLocalID. Empty string on invalid input.
    std::string derive_local_id(std::string_view app_secret_hex,
                                 std::string_view canvas_id);

    // Convenience: returns global_id + local_id (50 chars). Empty on error.
    std::string derive_share_code(std::string_view app_secret_hex,
                                   std::string_view canvas_id);

}  // namespace CanvasShareId
