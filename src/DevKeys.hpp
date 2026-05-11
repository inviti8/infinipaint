#pragma once
#include <filesystem>
#include <string>

// P0-C-DEV (DISTRIBUTION-PHASE0.md): dev-mode credential stand-in for
// the proper credential store coming in P0-C1/C3.
//
// Reads <configPath>/inkternity_dev_keys.json at startup. The file
// format matches dev_mint_token.py --save-state output, so the same
// keypair/canvas tuple drives both the host (which verifies tokens)
// and the dev mint script (which produces them):
//
//   {
//     "member_secret": "S..." or hex,
//     "member_pub":    "G..." or hex,
//     "app_secret":    hex,
//     "app_pub":       hex,
//     "canvas_id":     "uuid"
//   }
//
// Inkternity reads only the public fields + canvas_id; the secrets
// live alongside purely so dev_mint_token.py can mint matching tokens
// without manual copy/paste.
//
// When loaded, World::start_hosting auto-populates the canvas's
// subscription fields from these values, so EVERY hosted canvas runs
// in subscriber-only mode and accepts dev-minted tokens. This is
// strictly a Phase-0 dev convenience — production replaces it with
// portal-issued credentials + a Publish-for-Subscribers UI that sets
// the canvas's fields explicitly.
//
// Absence of the file = vanilla collab mode (existing behavior).
class DevKeys {
    public:
        // P0-C1 partial: ensure an app keypair exists at first run.
        // If <configPath>/inkternity_dev_keys.json doesn't exist OR
        // exists but lacks an app keypair, generate a fresh ed25519
        // pair (via tweetnacl crypto_sign_keypair + the OS RNG) and
        // write it. Existing fields (member_*, canvas_id, etc.) are
        // preserved when present. Idempotent — safe to call every run.
        // Call BEFORE load().
        bool ensure_app_keypair(const std::filesystem::path& configPath);

        // Returns true if the file existed and parsed cleanly. Logs
        // [USERINFO] on success/failure either way.
        bool load(const std::filesystem::path& configPath);

        bool is_loaded()                    const { return loaded; }
        const std::string& member_pubkey()  const { return memberPub; }
        const std::string& app_pubkey()     const { return appPub; }
        const std::string& canvas_id()      const { return canvasId; }

    private:
        bool loaded = false;
        std::string memberPub;
        std::string appPub;
        std::string canvasId;
};
