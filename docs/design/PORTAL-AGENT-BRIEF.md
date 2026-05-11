# Brief — Heavymeta Portal agent for Inkternity Phase 0

> **Use this when:** spawning a fresh agent in `C:\Users\surfa\Documents\metavinci\heavymeta_collective` to implement the portal side of Inkternity's Phase 0 distribution model. Paste the **TL;DR** through the **Files to read first** section into the agent's prompt.
>
> **What the agent is doing:** standing up the artist-side selling surface, the public per-canvas buy page, the Stripe webhook that mints subscriber tokens, and the artist app-key registration plumbing — entirely portal-side. The Inkternity desktop client is already token-gated and viewer-mode-clean; what's missing is the portal that issues those tokens from real purchases.
>
> **What this brief is NOT:** a substitute for the spec. The canonical design doc is `DISTRIBUTION-PHASE0.md`; the agent should read it (sections referenced inline below). This brief is the operational handoff — context, interop contract, and concrete subtasks.

## TL;DR for the portal agent

You're adding a new distribution channel to the Heavymeta Portal: artists can publish an Inkternity comic canvas, set a price, and sell access tokens that subscribers paste into their Inkternity desktop app to join the artist's live-hosted session as a read-only viewer.

The token format, the verifier, the desktop UI, and the dev-mint test harness are **already shipped on the Inkternity side** (`D:\repos\infinipaint`, see commits 559afd1 and onward). Your work is purely portal-side: schema, two artist dashboard pages, one public buy page, one Stripe webhook, and four API endpoints. About a week of NiceGUI + Stripe + SQLite work, mirroring the existing Metavinci credential pattern.

The trust model is **artist-sovereign** — tokens are signed by the artist's own member Stellar key (decrypted via the existing Banker + Guardian dual-key flow on the portal, identical risk profile to `/launch`'s Metavinci credential issuance). The host self-verifies against its own loaded keys. **No portal pubkey is bundled in Inkternity**; do not add one.

## Where this fits

- **Inkternity** = desktop comic-canvas app (C++ / Skia / SDL3, lives at `D:\repos\infinipaint`). Has WebRTC P2P live collab today; Phase 0 adds a token-gated viewer mode for paid subscribers.
- **Heavymeta Portal** (your repo) = the existing membership + Stripe + on-chain identity surface. Already issues Pintheon and Metavinci credentials via `/launch` using the `StellarSharedKeyTokenBuilder` / Banker+Guardian decrypt pattern.
- **Inkternity Server** (`D:\repos\inkternity-server`) = signaling + coturn deployment for the WebRTC layer. Independent of your work.

The Phase 0 design doc lives at `D:\repos\infinipaint\docs\design\DISTRIBUTION-PHASE0.md` — your work track is **§B** (sections 4-B + 9-B). Read the whole doc once for context; §3, §A.3, §B, §5, and §11 are the directly relevant slices.

## What's already shipped on the Inkternity side

You're interoperating with code that already exists. Don't re-spec it.

- **Five-check token verifier** (`infinipaint/src/Subscription/TokenVerifier.cpp`): parses the wire format, runs ed25519 signature verify (vendored tweetnacl, accepts both Stellar G... and raw-hex pubkey formats), and runs the four binding checks (identity, app, canvas, expiry). The `verify_token_for_host` function is the contract — its semantics ARE the spec.
- **Handshake extension** (`infinipaint/src/World.cpp` `start_hosting` recv callback + `init_client`): SERVER_INITIAL_DATA carries `(display_name, token)`. Host runs the verifier when `is_published_for_subscribers()` is true (= the loaded `.inkternity` file has `canvasId` set). Rejects via `setToDisconnect = true` on failure with a `[USERINFO]` log line naming the failure reason.
- **Viewer mode UI gating** (`infinipaint/src/Screens/PhoneDrawingProgramScreen.cpp`): toolbox + menu hidden, "Viewing live: <name>" indicator shown, defense-in-depth on the back button (no save). Subscriber's `ClientData.is_viewer = true` after accepted handshake.
- **File-format fields** (`infinipaint/src/World.hpp`): `canvasId` (UUID string), `artistMemberPubkey` (Stellar G... or hex string), `appPubkeyAtPublish` (hex string). Stamped at "Publish for Subscribers" time. Persisted in `.inkternity` files from format INFPNT000012 (= 0.11) onward.
- **Dev keys file** (`infinipaint/src/DevKeys.cpp`): reads `<configPath>/inkternity_dev_keys.json` at startup. The format is exactly what `dev_mint_token.py --save-state` produces. **Your portal's job in production is to replace the manual dev-keys file with portal-issued credentials** (Phase 0.5+ on the desktop side).
- **Dev mint script** (`inkternity-server/scripts/dev_mint_token.py`): ed25519-signs Phase 0 tokens locally for testing. Use this as the reference implementation for the portal's signing helper. Validated end-to-end: tokens it produces verify correctly against the desktop app.

## What you're building (P0-B subtasks)

Numbered to match `DISTRIBUTION-PHASE0.md` §9-B.

### B1 — Schema (3 SQLite tables)

Add to the existing `data/collective.db` via the standard schema-add pattern in `db.py`:

```sql
CREATE TABLE inkternity_app_keys (
    artist_user_id INTEGER NOT NULL,
    app_pubkey     TEXT    NOT NULL,        -- 32-byte ed25519, hex-encoded (64 chars)
    label          TEXT,                    -- artist-supplied: "Studio iMac", "Laptop"
    registered_at  INTEGER NOT NULL,
    PRIMARY KEY (artist_user_id, app_pubkey),
    FOREIGN KEY (artist_user_id) REFERENCES users(id)
);

CREATE TABLE inkternity_canvases (
    canvas_id          TEXT    PRIMARY KEY,  -- UUIDv4
    artist_user_id     INTEGER NOT NULL,
    app_pubkey         TEXT    NOT NULL,     -- which install will host this canvas
    title              TEXT    NOT NULL,
    description        TEXT,
    price_usd          REAL    NOT NULL,
    stripe_product_id  TEXT,
    created_at         INTEGER NOT NULL,
    FOREIGN KEY (artist_user_id) REFERENCES users(id),
    FOREIGN KEY (artist_user_id, app_pubkey) REFERENCES inkternity_app_keys(artist_user_id, app_pubkey)
);

CREATE TABLE inkternity_tokens (
    token_id                 TEXT    PRIMARY KEY,  -- UUIDv4 (portal-internal id, not the wire token)
    canvas_id                TEXT    NOT NULL,
    app_pubkey               TEXT    NOT NULL,     -- which install this token grants access to (= canvas's app_pubkey at mint time)
    subscriber_email         TEXT,                 -- buyer; can be null for anonymous flows later
    subscriber_stellar_addr  TEXT,                 -- optional, currently always null in Phase 0
    issued_at                INTEGER NOT NULL,
    expires_at               INTEGER,              -- nullable; null = no expiry
    stripe_payment_intent_id TEXT,
    signed_payload           TEXT    NOT NULL,     -- the actual wire-format token shown to subscriber
    FOREIGN KEY (canvas_id) REFERENCES inkternity_canvases(canvas_id)
);
```

### B2 — Token signing helper (the load-bearing piece)

Function signature (Python, suggested location: `payments/inkternity_tokens.py` or extend `launch.py`):

```python
def mint_inkternity_token(
    artist_user_id: int,
    canvas_id: str,
    app_pubkey: str,
    subscriber_email: str | None,
    expires_in_seconds: int | None = None,
) -> str:
    """Returns the wire-format token: <base64url(sig)>.<base64url(payload)>"""
```

Implementation:
1. Authenticate as the artist via the existing Banker + Guardian dual-key flow (see `enrollment.py` for the canonical decrypt pattern; `launch.py`'s `generate_metavinci_credentials` is the closest existing caller). Gets you the artist's plaintext member Stellar secret in process memory.
2. Look up the artist's member Stellar pubkey from `users` (already on every paid member).
3. Build the payload (sorted compact JSON, single-letter keys):
   ```json
   {"a":"GAB...","c":"<canvas_id>","e":<expires_at>,"i":<now>,"k":"<app_pubkey>","sub":"<sha256(email)>"}
   ```
   Omit `e` if no expiry. `sub` is `hashlib.sha256(subscriber_email.encode()).hexdigest()` or omitted when no email.
4. Sign the UTF-8 JSON bytes with the artist's member secret (Stellar Ed25519). The `stellar_sdk` `Keypair.sign(payload_bytes)` does this — output is 64 raw bytes.
5. Wire format: `base64url(signature).base64url(payload)`, no padding (`b'='.rstrip(b'=')`).
6. **Discard the plaintext secret immediately.** Use `try/finally` to zero the variable; treat it like the Metavinci credential flow does.

The reference implementation is `inkternity-server/scripts/dev_mint_token.py` — the `mint()` function is exactly the algorithm to mirror. Your output MUST verify with `dev_mint_token.py --verify <token>` (sanity check during development).

### B3 — Artist `/dashboard/inkternity/apps`

NiceGUI page mirroring the structure of existing `/dashboard/*` routes in `main.py`:

- Lists the artist's registered Inkternity installs (rows from `inkternity_app_keys` for the current user).
- "Register new install" form: paste field for `app_pubkey` (32-byte ed25519 hex, 64 chars; validate length + hex), text field for `label`, Save button.
- "Remove" button per row (deletes from `inkternity_app_keys`; warn that any unused tokens addressed to that install will fail to verify).

Auth: existing portal session (`app.storage.user`).

### B4 — Artist `/dashboard/inkternity` (canvas list + create)

NiceGUI page:

- Lists the artist's canvases (`inkternity_canvases` for current user).
- "Add canvas" form: title, description, price USD, dropdown of registered app pubkeys (= which install will host).
- On Add:
  - Insert `inkternity_canvases` row with a fresh UUIDv4 canvas_id.
  - Create a Stripe Connect product (mirror commissions flow in `payments/`); store `stripe_product_id`.
  - Show the public buy URL: `<base>/inkternity/canvas/<canvas_id>`.
- "Edit" + "Delete" per row.

### B5 — Public `/inkternity/canvas/{canvas_id}` (buy page)

Buyer-facing, no portal account required (mirror `/commission/{moniker_slug}` in `main.py`):

- Page shows canvas title, description, price, artist's display name (or moniker).
- Email field (the subscriber's email, used for the token).
- "Buy access" button → Stripe Connect Checkout session, success URL routes back to a "Thanks, here's your token" page.

### B6 — Stripe webhook (mints token, persists, emails)

Extend the existing Stripe webhook handler:

- On `payment_intent.succeeded` for an Inkternity-canvas product:
  - Look up the canvas + artist.
  - Call `mint_inkternity_token(...)` with the buyer's email and a configurable expiry (default: no expiry for Phase 0; per-canvas expiry override is a Phase 0.5 dial).
  - Insert `inkternity_tokens` row.
  - Send email to buyer: subject "Your Inkternity access token for <title>", body includes the token + brief instructions ("paste this into Inkternity → Menu → Connect when the artist is live").
  - Render a success-page version of the same info for the redirect.

### B7 — API endpoints Inkternity calls

These don't ship in Phase 0's first cut (the desktop has no portal client yet — it uses `inkternity_dev_keys.json`), but they're the contract for Phase 0.5:

- `POST /api/inkternity/canvases` — body `{title, price_usd, app_pubkey}`, returns `{canvas_id, buy_url}`. Used when the desktop app's "Publish for Subscribers" lands.
- `GET /api/inkternity/apps/{app_pubkey}` — returns 200 if registered, 404 if not. Used by Inkternity to confirm the local app pubkey is registered before allowing publish.

Auth: bearer-token (mirror `BOT_API_TOKEN` pattern from `telegram_bot/portal_client.py`); generate per-install at desktop credential issuance time.

## Critical interop contract — token wire format

This MUST match the desktop verifier byte-for-byte. The desktop side is locked; the portal must match.

```
<base64url(64-byte ed25519 signature)> "." <base64url(json payload)>
```

- **base64url**: RFC 4648 §5, no padding (strip trailing `=`).
- **Payload**: `json.dumps(payload, separators=(",",":"), sort_keys=True).encode()`. Sorted keys are required so the verifier reconstructs the same bytes.
- **Payload schema**:
  ```json
  {
    "a":   "<artist member pubkey, Stellar G... 56-char OR raw hex 64-char>",
    "c":   "<canvas UUID>",
    "i":   <issued_at unix seconds, integer>,
    "k":   "<artist app pubkey, raw hex 64-char>",
    "e":   <expires_at unix seconds, integer; OMIT for no expiry>,
    "sub": "<sha256 of subscriber email, hex; OMIT if no email>"
  }
  ```
- **Signature**: ed25519 over the JSON payload bytes (NOT over the base64url-encoded form). The signer is the artist's member key.

Verify with `python inkternity-server/scripts/dev_mint_token.py --verify <token>` — exits 0 with the parsed payload if valid, exits 2 if not.

## Existing patterns to mirror inside `heavymeta_collective`

These already exist and answer most "how do I X" questions:

| Need | Look at |
|---|---|
| NiceGUI page wiring + auth | `main.py` route handlers (e.g. `/launch`, `/dashboard/commissions`) |
| Stripe Connect product + checkout | `payments/` module + commissions flow |
| Stripe webhook handling | The existing `payment_intent.succeeded` handler (or `checkout.session.completed`) |
| Banker + Guardian decrypt of member secret | `enrollment.py` setup + `launch.py` `generate_metavinci_credentials` |
| App credential issuance pattern | `launch.py` `generate_metavinci_credentials` (Stellar shared-key tokens with caveats) |
| Email sending | Existing Mailtrap setup; look at how commission notification emails work |
| SQLite table addition | `db.py` schema versioning |
| API endpoint with bearer-token auth | `telegram_bot/portal_client.py` (consumer) + matching server-side handler |
| Tier definitions / pricing | `static/tiers.json` |
| Apps catalog | `static/apps.json` (you'll add an `inkternity` entry) |

## Things to deliberately NOT do

- **Don't sign tokens with a portal master key.** The trust model is artist-sovereign — every token is signed with the buyer's artist's individual member key, decrypted briefly on the portal. This is exactly what `/launch`'s Metavinci credential flow already does for an app credential; you're applying the same pattern per-purchase instead of per-/launch-call.
- **Don't bundle a portal verification key into the Inkternity binary.** The desktop side already verifies against its own loaded artist member pubkey (via `inkternity_dev_keys.json` in dev, via portal-issued credentials in P0-C1/C3 future work). No portal pubkey appears anywhere in Inkternity.
- **Don't add roster gating.** The portal already gates membership at token-mint time (won't issue tokens to lapsed members). Host-side roster check was considered and explicitly removed from Phase 0 — see `DISTRIBUTION-PHASE0.md` §A.3 last paragraph.
- **Don't add recurring subscription.** Phase 0 is one-time per-canvas Stripe Connect only. Recurring is `DISTRIBUTION.md` §6.2 territory; needs new portal table + handler design that's out of scope.
- **Don't add per-subscriber encryption / IPFS / Lepus.** Phase 0 is plain plaintext WebRTC sessions to the artist's own host. Per-subscriber encryption is `DISTRIBUTION.md` §5.3 (Phase 1+ work).
- **Don't change the token wire format.** It's locked to the desktop verifier. If you find ANY reason the format needs to change, escalate — coordinated change across both repos.
- **Don't try to share a database with Inkternity.** They're separate processes on potentially different machines. The portal owns its SQLite; Inkternity reads `inkternity_dev_keys.json` (Phase 0) and will eventually read portal-issued credentials (Phase 0.5).
- **Don't include the artist's plaintext Stellar secret in any persisted state, log, or HTTP response.** It exists only briefly in `mint_inkternity_token`'s memory and is discarded.

## Test harness — how to verify end-to-end

You can fully exercise the desktop side without changing any Inkternity code:

1. Install Inkternity from `D:\repos\infinipaint\build\Release\inkternity.exe`.
2. Generate a dev keys file (this stand-in is the credential-store the portal will eventually replace):
   ```bash
   cd D:\repos\inkternity-server
   python scripts/dev_mint_token.py --gen-keys --save-state "$env:APPDATA\ErrorAtLine0\infinipaint\inkternity_dev_keys.json"
   ```
   Save the printed `member_pub` and `canvas_id` — you'll need them for the portal-side mint.
3. **From your portal-side `mint_inkternity_token` function**: produce a token using the same `member_pub` (i.e., simulate that the artist's member key on the portal matches the one in the dev keys file). Your token is structurally identical to what the dev mint script produces.
4. Verify your portal-minted token with the dev script:
   ```bash
   python scripts/dev_mint_token.py --verify <your-token>
   ```
   Should print `OK` and the parsed payload.
5. Optional: run the full WebRTC flow per `DISTRIBUTION-PHASE0.md` §7.

If your portal token verifies via the dev script, it will verify in Inkternity's host. The wire format is the entire interop contract.

## Suggested order

1. **B2 first** — token signing helper. Pure function, easy to unit-test, validates against `dev_mint_token.py --verify` immediately. About 1 day.
2. **B1** — schema. ~½ day.
3. **B3** — app-key registration page. ~1 day.
4. **B4** — canvas list/create page. Coordinate with existing Stripe product creation pattern. ~1.5 days.
5. **B5** — public buy page. Mirrors `/commission/...`. ~1 day.
6. **B6** — webhook → mint → email. Most integration-heavy. ~1.5 days.
7. **B7** — API endpoints. Forward-compat for desktop P0-C4. ~½ day.

Total: ~1 week of focused work. Heavily reuses existing code; the new logic is concentrated in B2 and B6.

## Files to read first (in this order)

1. `D:\repos\infinipaint\docs\design\DISTRIBUTION-PHASE0.md` — full spec (canonical).
2. `D:\repos\inkternity-server\scripts\dev_mint_token.py` — reference implementation of the signing algorithm.
3. `D:\repos\infinipaint\src\Subscription\TokenVerifier.cpp` — the verifier you're producing tokens for.
4. `C:\Users\surfa\Documents\metavinci\heavymeta_collective\launch.py` — the credential-issuance pattern to mirror (especially `generate_metavinci_credentials`).
5. `C:\Users\surfa\Documents\metavinci\heavymeta_collective\enrollment.py` — the Banker + Guardian decrypt pattern.
6. `C:\Users\surfa\Documents\metavinci\heavymeta_collective\db.py` — schema additions.
7. `C:\Users\surfa\Documents\metavinci\heavymeta_collective\COMMISSIONS.md` + the commissions code paths in `main.py` — closest existing analog (Stripe Connect, public landing page, no recurring).
8. `C:\Users\surfa\Documents\metavinci\heavymeta_collective\static\apps.json` + `static\tiers.json` — small additions for the Inkternity entry.

## Open questions to resolve as you go

These are flagged in `DISTRIBUTION-PHASE0.md` §11 — do not pick a default silently, surface them:

- **Stripe Connect application-fee policy for Inkternity sales** — match commissions (0% paid tier, 5% free)? Or new tier? Ask.
- **Token expiry default** — Phase 0 is OK with no expiry, but any per-canvas knob the artist gets in B4 should be specified.
- **App-keypair re-derivation if user reinstalls Inkternity** — the desktop side's behavior is to regenerate, requiring re-registration on the portal. Documented limitation; the portal should make removing + re-adding an app pubkey friction-free.

## Out of scope for the portal Phase 0

Forward-pointer to `DISTRIBUTION.md` (the long-form thesis):

- Per-subscriber encrypted Issue payloads.
- IPFS pinning / Pintheon integration.
- Lepus datapods.
- Recurring subscription billing.
- "My subscriptions" subscriber-side dashboard.
- Refund / dispute handling beyond what Stripe Connect provides natively.

---

When the agent finishes, run the **end-to-end test** in §"Test harness" above before declaring done. The wire-format match is the whole acceptance criterion for B2; the rest is standard portal-shape work.
