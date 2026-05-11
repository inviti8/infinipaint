# Inkternity — Distribution Phase 0

> **Audience:** the agent (and any human contributor) coordinating Phase 0 between Inkternity, the Heavymeta Portal, and the network infrastructure HEAVYMETA owns.
>
> **Goal of this doc:** define the minimum-viable artist-subscriber path that ships fast, leverages everything already built, and slots cleanly into the longer-term `DISTRIBUTION.md` thesis without repainting any of it later.

## 1. Product summary

Phase 0 turns Inkternity's existing live-collab session into a tokenized, viewer-only "live canvas access" surface for paying subscribers. The artist hosts a canvas as they do today; subscribers buy a per-canvas access token from the Heavymeta Portal; the host's Inkternity verifies that token at the join handshake and lets the subscriber in as a read-only viewer.

No new IPFS, no datapods, no per-subscriber encryption, no recurring billing, no portal session-routing, no replay. All of that is forward-loaded into the full `DISTRIBUTION.md` design. Phase 0 is the smallest cut that proves: *artists can sell access to their living canvas, end to end, on HEAVYMETA infrastructure*.

## 2. Phase-0 decisions

| Decision | Choice | Why |
|---|---|---|
| **Availability** | Live-only — artist must be hosting | No replay layer needed. Closes the design space dramatically. |
| **Subscriber role** | Viewer-only — no editing | Defines a clean boundary between "collaborator" (existing Collab mode) and "subscriber" (new). |
| **Token granularity** | One token per canvas (issue) | Simplest commerce model. Maps 1-to-1 with Stripe Connect line items. |
| **Sharing mechanism** | None new — artist still shares lobby address out-of-band | Zero portal-side discovery work. Subscriber pastes lobby address + token at join time. |
| **Token lifetime** | Simplest first; optional `expires_at` field supported | Lets artist offer "lifetime access to this canvas" or "30 days" without changing the protocol. |
| **Infrastructure** | HEAVYMETA-owned signaling + TURN, from day one | Independence from infinipaint.com. Long-term ownership; ~1-2 days devops. |

## 3. Inheritance

What Phase 0 reuses unchanged:

- **Existing Collab P2P stack** — WebRTC, `NetObj` replication, server-authoritative model, `ClientData` per-peer state, chat, peer list, jump-to-peer.
- **`SERVER_INITIAL_DATA` handshake** — extended with one field (the token).
- **Display-name + cursor-color UX** — viewers appear in the peer list like any other client.
- **Reader mode** — the natural read surface for viewers; if the artist has a curated waypoint walk, viewers experience it.
- **`.inkternity` file format** — extended with one optional field (the canvas-registration id).

What Phase 0 introduces and what it deliberately does NOT introduce:

| Introduces | Does NOT introduce (yet) |
|---|---|
| Per-canvas registration on the portal | Per-subscriber encrypted payloads |
| Inkternity per-install **app keypair** generated on first run | IPFS / Pintheon distribution |
| Artist app-pubkey registration with the portal | Lepus datapods |
| Access tokens signed by **artist's member Stellar key**, bound to artist app pubkey | Recurring subscriptions |
| Token verification at host accept-connection (host self-verifies against its own keys) | Per-tier branching narratives |
| Viewer mode (read-only client) | Per-canvas signing keys + delegation certs (Phase 0.5) |
| HEAVYMETA-owned signaling + TURN | Time-locked reveals, watermarks, annotations |
| Stripe Connect one-time per-canvas purchase | Subscriber-stellar-key binding |

The discipline: each new piece in Phase 0 is something the larger `DISTRIBUTION.md` plan also needs. Nothing here is throwaway.

## 4. Work tracks

Four parallel tracks. All four can start immediately; D and C have a soft dependency on B (the portal needs to know how to issue a token before the desktop apps can verify one).

### A. HEAVYMETA-owned signaling + TURN

**Why now:** infinipaint.com's `signalserver.infinipaint.com:8000` and `turn.infinipaint.com:3478` are friendly third-party infrastructure. They could vanish. They are also branded with someone else's domain. Phase 0 is the right moment to migrate; the work is small, the dependency curve only gets harder later.

**Investigation outcome — hvym_tunnler does NOT fit here.** HEAVYMETA's existing tunneling service (`C:\Users\surfa\Documents\metavinci\hvym_tunnler`, deployed at `tunnel.hvym.link`) is an HTTP-only reverse tunnel — no UDP (so it cannot relay TURN traffic), no WebSocket passthrough (it serializes each request as a JSON envelope, so it cannot proxy the WebRTC signaling WebSocket either). It's the right tool for publishing artist HTTP services like Pintheon galleries; it is not a substitute for either signaling or coturn. See §A.1 below for the one valid future composition.

**What we deploy:**
- Provision a small VPS (DigitalOcean / Hetzner / similar, $5–10/mo). HEAVYMETA may already have suitable hardware.
- Deploy `coturn` (standard open-source TURN server). One binary, one config file. Tested for years, well-documented.
- Deploy `libdatachannel`'s stock signaling server. The Inkternity client uses `libdatachannel` (paullouisageneau/libdatachannel) for WebRTC, and the upstream project ships a reference signaling server (`signaling-server-python` and `signaling-server-rust`) that speaks the exact protocol Inkternity expects: WSS to `/<globalID>`, JSON `{id, type, description}` for offer/answer and `{id, type:"candidate", candidate, mid}` for ICE. We do not need to write any custom code, contact the InfiniPaint developer, or maintain a fork — just deploy the upstream reference.
- DNS: `signal.heavymeta.art`, `turn.heavymeta.art` (subdomains under existing heavymeta.art).
- TLS: Let's Encrypt for the WSS signaling endpoint.
- Update Inkternity build's `assets/data/config/default_p2p.json` to point to the new endpoints.

**Effort:** ~1 day. No code authored; both pieces are stock open-source binaries with config files. ~$10/month ongoing infrastructure cost.

**Output:** `default_p2p.json` shipping with HEAVYMETA endpoints. Inkternity hosts and clients route entirely through HEAVYMETA infrastructure for Phase 0.

#### A.1 Deployment recipe — what we write vs. what we get free

There is no off-the-shelf "Inkternity infrastructure" package. We compose two stock open-source pieces with a thin wrapper, mirroring how `hvym_tunnler` already deploys (`scripts/vps_startup.sh` + `docker-compose.yml` + `Dockerfile`).

**Off-the-shelf (no code we write):**

| Component | What | Source |
|---|---|---|
| TURN server | `coturn` — the canonical open-source TURN/STUN server. Single binary, single config file (`/etc/turnserver.conf`). | Ubuntu/Debian apt: `apt install coturn`. Or `docker.io/coturn/coturn` image. |
| Signaling server | `libdatachannel` reference signaling server. Two flavors: `signaling-server-python` (simple, ~200 LoC, asyncio + websockets) or `signaling-server-rust` (faster, single binary). Both speak Inkternity's exact protocol unmodified. | https://github.com/paullouisageneau/libdatachannel — reference signaling subdirectory. |
| TLS reverse proxy | Caddy (preferred — auto-Let's-Encrypt) or nginx + certbot (matches hvym_tunnler's existing pattern). | `caddy/caddy` image, or HEAVYMETA's existing nginx setup. |
| Process supervision | systemd or Docker Compose. | OS-native or `docker compose`. |

**What we write (small wrapper repo, suggested name `hvym-inkternity-infra`):**

```
hvym-inkternity-infra/
├── README.md                   # "How to stand up Inkternity infra on a fresh VPS"
├── docker-compose.yml          # signaling + coturn + caddy services
├── caddy/
│   └── Caddyfile               # reverse-proxies signal.heavymeta.art → libdatachannel signaling
├── coturn/
│   └── turnserver.conf         # TURN config: realm, static-auth-secret, cert paths
├── signaling/
│   └── Dockerfile              # pulls libdatachannel reference signaling, runs it
└── scripts/
    ├── vps_startup.sh          # one-shot fresh-VPS bootstrap (apt update, install docker,
    │                           # clone this repo, start compose stack)
    └── rotate_turn_secret.sh   # regenerate coturn static-auth-secret + push to default_p2p.json
```

That's roughly **150–300 lines of YAML/conf/shell across six files**. Most of it is boilerplate adapted from the corresponding hvym_tunnler files (which already exist and work). The Dockerfile for the signaling server is ~10 lines (clone libdatachannel, build, expose port 8000). The `docker-compose.yml` is ~40 lines defining three services. The `Caddyfile` is ~15 lines (one site, automatic TLS). The coturn config is ~30 lines (most of it commented examples).

**Reference docker-compose shape** (illustrative, not prescriptive):

```yaml
services:
  signaling:
    build: ./signaling
    restart: unless-stopped
    expose: ["8000"]

  coturn:
    image: coturn/coturn:latest
    restart: unless-stopped
    network_mode: host
    volumes:
      - ./coturn/turnserver.conf:/etc/coturn/turnserver.conf:ro
      - /etc/letsencrypt/live/turn.heavymeta.art:/certs:ro

  caddy:
    image: caddy:latest
    restart: unless-stopped
    ports: ["443:443", "80:80"]
    volumes:
      - ./caddy/Caddyfile:/etc/caddy/Caddyfile:ro
      - caddy_data:/data
      - caddy_config:/config

volumes:
  caddy_data:
  caddy_config:
```

**Reference Caddyfile** (one-line TLS for the signaling endpoint):

```
signal.heavymeta.art {
    reverse_proxy /* signaling:8000
}
```

**Effort breakdown:**

- Cribbing `vps_startup.sh` and `docker-compose.yml` shape from hvym_tunnler: ~1 hour.
- Writing the signaling Dockerfile: ~30 min (it's a `git clone` + `pip install` + `python3 signaling.py`).
- Writing `turnserver.conf` from coturn's documented template: ~30 min.
- DNS + Let's Encrypt + first deployment + smoke test from a fresh Inkternity client: ~3–4 hours including unfamiliar-system fumbling.
- Total: realistic 1 day for one engineer who's done a Docker + Caddy + VPS deployment before. Two days if learning any of those tools simultaneously.

**The path with the shortest critical line of code:** if HEAVYMETA's existing `tunnel.hvym.link` VPS has spare capacity (it likely does — hvym_tunnler is small), the signaling+coturn stack co-locates there. Same Caddy/certbot setup, additional subdomains under `heavymeta.art`, no new VPS provisioning. This drops the effort to roughly half a day.

**What we do NOT need:**
- No custom protocol code — Inkternity already speaks the libdatachannel signaling protocol.
- No custom TURN extensions — coturn handles standard STUN+TURN out of the box.
- No clustering or HA in Phase 0 — single-VPS is fine for "tens to low-hundreds of concurrent sessions" (libdatachannel signaling is asyncio, coturn handles thousands of simultaneous relays on modest hardware).
- No monitoring stack in Phase 0 — `docker logs` is sufficient. Add Grafana/Prometheus when usage justifies it.

Once `hvym-inkternity-infra` exists, it becomes the canonical "spin up Inkternity infra" recipe — the next deployment (region, redundancy, dev-vs-prod) is a `git clone` + `vps_startup.sh` away.

#### A.2 hvym_tunnler — the Phase 0.5 composition

Although hvym_tunnler can't carry WebRTC, it does solve the most painful UX gap Phase 0 leaves open: **the artist still has to copy-paste the lobby address out-of-band every time they go live** (Phase 0 §8 risk). A Phase 0.5 follow-up can use hvym_tunnler to fix this without changing anything in §A above:

```
ARTIST (when hosting):
  Inkternity opens a tiny local HTTP server bound to localhost:N exposing
    GET /inkternity/lobby → { "lobby": "<currentGlobalID + serverLocalID>" }
  hvym_tunnler client (from Metavinci) publishes that local endpoint
  publicly at https://<artist_stellar_addr>.tunnel.hvym.link/inkternity/lobby

SUBSCRIBER (in Inkternity):
  Pastes only the artist's stellar address (or just selects from a
  "subscribed artists" list).
  App fetches https://<artist_stellar_addr>.tunnel.hvym.link/inkternity/lobby,
  presenting its existing token as a Bearer header for hvym_tunnler's
  Stellar-JWT auth gate.
  If the artist is live: lobby address returned, normal connect proceeds.
  If not: clean "artist is not live" state.
```

This composes the three pieces for what each is best at: hvym_tunnler for HTTP-based discovery, libdatachannel signaling + coturn for the WebRTC session itself, and Stellar JWTs for cross-app auth. Phase 0 ships without it; Phase 0.5 adds it as a strict improvement.

#### A.3 Cross-app auth alignment

The Phase 0 token uses ed25519 (`alg: EdDSA`), the same signature scheme as `hvym_tunnler/app/auth/jwt_verifier.py`. Verification primitives are shared across HEAVYMETA apps; future hardening can switch the wire format to fully JWT-conformant claims without changing the trust model.

**The Phase 0 trust model is artist-sovereign, not portal-rooted.** Tokens are signed by the artist's own member Stellar key (decrypted briefly on the portal via the existing Banker+Guardian dual-key flow) and bound to the artist's per-install Inkternity app pubkey. The host (which IS the artist) self-verifies tokens against its own member pubkey + own app pubkey. **No portal pubkey is bundled in the Inkternity binary**, no on-chain roster lookup is required at verification time, no third-party trust path exists between artist and subscriber.

Three keys participate, but only one signs:

| Key | Where it lives | Touched when |
|---|---|---|
| Artist **member key** (Stellar identity, on `hvym_roster`) | Encrypted on portal via Banker + Guardian | One signature per token mint (decrypted in webhook handler, used, discarded — same pattern as `/launch` for Metavinci credentials) |
| Artist **Inkternity app key** | Generated by Inkternity on first run, stored locally encrypted with portal credentials. Pubkey copied to portal at app-registration time. | Never used to sign Phase 0 tokens — referenced by pubkey only |
| **Subscriber identity** (Stripe email, optional Stellar pubkey) | Portal | Hashed into `payload.sub` for audit; not directly used in v1 token crypto |

The app-key binding gives free per-install revocation: if the artist retires a machine, they re-register a new app pubkey on the portal; old tokens carry the old `k` and the new install's `own_app_pubkey` won't match.

Forward path: per-canvas signing keys + master-key-signed delegation certs slot in on top of this without touching the verification primitive (`payload.k` becomes the canvas key instead of the app key, with a delegation cert chain). See §10 for the deferral.

A second forward path: host-side `hvym_roster` membership check (the pattern `hvym_tunnler/app/auth/roster.py` and the metavinci daemon use). Phase 0 doesn't need it — the portal already gates token issuance on current membership, so any token reaching a host's verification step came from a then-active member. Adding host-side roster polling would only police tokens issued before a membership lapse, an edge case worth deferring until there's a concrete reason. Reusing the existing roster-sync code in Phase 1+ is straightforward when that reason arrives.

### B. Portal: canvas registration + token issuance

**Why now:** the canvas-id, the artist app-pubkey registration, and the access token are the three new primitives Phase 0 introduces; they all live on the portal.

**What:**
- New SQLite table `inkternity_app_keys` — one row per (artist, machine):
  ```
  artist_user_id INTEGER,             -- FK to portal users
  app_pubkey TEXT NOT NULL,           -- 32-byte ed25519, hex-encoded
  label TEXT,                         -- artist-supplied: "Studio iMac", "Laptop"
  registered_at INTEGER,
  PRIMARY KEY (artist_user_id, app_pubkey)
  ```
  An artist can register multiple app pubkeys (multiple installs); tokens carry one specific pubkey, so a subscriber connecting to a specific install gets a specific token.
- New SQLite table `inkternity_canvases`:
  ```
  canvas_id TEXT PRIMARY KEY,         -- UUIDv4
  artist_user_id INTEGER,             -- FK to portal users
  title TEXT,
  description TEXT,
  price_usd REAL,
  stripe_product_id TEXT,
  created_at INTEGER
  ```
- New SQLite table `inkternity_tokens`:
  ```
  token_id TEXT PRIMARY KEY,
  canvas_id TEXT,                     -- FK
  app_pubkey TEXT,                    -- which install this token grants access to
  subscriber_email TEXT,              -- buyer; can be anonymous
  subscriber_stellar_addr TEXT,       -- optional, may be null
  issued_at INTEGER,
  expires_at INTEGER,                 -- nullable; null = no expiry
  stripe_payment_intent_id TEXT,
  signed_payload TEXT                 -- the actual token bytes shown to subscriber
  ```
- New artist-side route `/dashboard/inkternity/apps` — list registered Inkternity installs, "Register new" accepts the app pubkey + a friendly label.
- New artist-side route `/dashboard/inkternity` — list canvases, "Add new" creates a canvas record + Stripe Connect product, returns a public buy URL. Each canvas may target a default app pubkey (most artists have one install).
- New public route `/inkternity/canvas/{canvas_id}` — landing page with title/description/price + Stripe Checkout button.
- Stripe Connect webhook on payment success: mints a token, stores it in `inkternity_tokens`, shows it in-browser + emails it.
- **Token signing**: webhook handler authenticates as the artist (via the existing Banker + Guardian dual-key flow), decrypts the artist's member Stellar secret in-memory, signs the payload, immediately discards the plaintext secret. Same pattern and risk profile as today's `/launch` flow that issues Metavinci credentials.
- Token format: `ed25519_signature | base64url(json_payload)` where:
  ```json
  { "a": "GAB...artist_member_pubkey",
    "k": "abcd...artist_app_pubkey_hex",
    "c": "uuid-canvas-id",
    "i": 1730150400,            // issued_at
    "e": 1761686400,            // expires_at (optional, omit for forever)
    "sub": "sha256(subscriber_email)" }
  ```
- Token shown to subscriber in-browser + emailed (mirrors `generate_metavinci_credentials` UX).
- **No portal pubkey is published or bundled anywhere.** Verification keys are: `payload.a` (artist member pubkey, the host already knows its own) and `payload.k` (artist app pubkey, the host already knows its own).

**Effort:** ~1 week including Stripe Connect product creation, webhook, UI in NiceGUI. Reuses existing payment/email/auth plumbing.

**Output:** an artist can register an Inkternity install, create a canvas listing, set a price, share the buy link; a subscriber can buy and receive a verifiable token.

### C. Inkternity (artist side): app key + canvas registration + subscriber-only host mode

**Why now:** the host is where token verification happens. The artist needs (1) a per-install app keypair, (2) a way to register a canvas with the portal, (3) a host mode that gates new connections on a valid token.

**What:**
- **App keypair generation.** On first run after portal credentials are installed, Inkternity generates an ed25519 keypair (`crypto_sign_keypair` from libsodium). Stored locally encrypted alongside the portal credentials in the existing credential store. Pubkey exposed in **lobby settings** (`FileSelectScreen` settings panel — *not* the per-canvas drawing view, since the keypair is per-install and persists across all canvases). UI: `Lobby → Settings → HEAVYMETA → Inkternity App Key` with a "Copy public key" button. The artist pastes this pubkey into the portal's app-registration page (§B).
- **Format bump 0.11 → 0.12** on the `.inkternity` file: adds `canvas_id` (UUID), `artist_member_pubkey` (Stellar address), and `app_pubkey_at_publish` (which install this canvas was published from). All three default null/empty for unpublished canvases. Save/load gated on version >= 0.12 same way TRANSITIONS bumped.
- **`Menu → Publish for Subscribers...` action:**
  1. Reads artist credentials from local credential store (set up via portal `/launch` — this leverages the existing Metavinci credential pattern).
  2. Confirms the local app pubkey is registered with the portal (calls `GET /api/inkternity/apps/{app_pubkey}`); if not registered, surfaces a dialog instructing the artist to register it on the portal first.
  3. POSTs `{title, price_usd, app_pubkey}` to portal `/api/inkternity/canvases`.
  4. Receives back `{canvas_id, buy_url}`.
  5. Stamps `canvas_id`, `artist_member_pubkey`, and `app_pubkey_at_publish` (= the local app pubkey) into the file. Saves.
  6. Shows a dialog with the buy URL the artist copies and shares.
- **Subscriber-only host mode.** When the canvas has a non-null `canvas_id` AND the artist hosts it, the host runs in subscriber-only mode. Existing host UX unchanged in shape.
- **Handshake extension.** `SERVER_INITIAL_DATA` gains an optional `token` field. On receipt, the host runs the five-check verification:
  1. **Signature**: `ed25519.verify(payload.a, signature, payload_bytes)` — the token's claimed signer (artist member pubkey) is the verification key.
  2. **Identity binding**: `payload.a == host.own_member_pubkey` — token must be addressed to THIS artist.
  3. **App binding**: `payload.k == host.own_app_pubkey` — token must be issued for THIS install.
  4. **Canvas binding**: `payload.c == host.canvas_id` — token must match the open `.inkternity` file.
  5. **Expiry**: `payload.e > now()` if `e` is set.
  - On any failure: reject the connection cleanly (close WebRTC peer, optional error message to subscriber).
  - On success: proceed with `CLIENT_INITIAL_DATA` as today, plus mark the new client's `ClientData` with `is_viewer=true`.
- A "Subscribers" indicator in the existing peer list shows which connected peers are viewers vs collaborators.
- Artist still copies the lobby address from `Menu → Lobby Info` and shares out-of-band.

**Effort:** 4–5 days. The format bump + menu action + handshake extension + viewer-flag + app-keypair-generation are all small touches in well-understood files (`World.cpp`, `Toolbar.cpp`, `Waypoint.hpp` style version-gating). Add ~½ day for the libsodium integration (already a transitive dep via libdatachannel, just exposing it).

**Output:** an artist with portal-issued credentials can register an Inkternity install with the portal, publish a canvas, host it, and have it accept only tokens issued by themselves to subscribers of that canvas.

### D. Inkternity (subscriber side): connect-with-token flow + viewer mode

**Why now:** the join surface is the other half of the trust handshake. Viewer-mode UI gating is the smallest credible "viewer-only" implementation.

**What:**
- `Menu → Connect (with token)...` — new menu item alongside existing `Connect`.
- Dialog: two text fields, "Lobby Address" and "Access Token." Connect button.
- On Connect: opens client `World` with `isClient=true` AND `subscriberToken=<token>`. The token is included in the new client's `SERVER_INITIAL_DATA` send.
- If the host rejects (token bad, canvas mismatch, expired): show a clear error toast and close the world cleanly.
- If the host accepts: the client's `ClientData` arrives with `is_viewer=true` set by the host's NetObj update.
- **Viewer mode UI gating** when `ownClientData->is_viewer()`:
  - Hide the entire toolbox (no brush, no eraser, no waypoint tool, no edit tool — none of the editing surface). Keep the camera controls, zoom, pan, reader-mode toggle, and chat.
  - Hide the file menu items that mutate the file (Save, Save As, Open replaces this world).
  - Show a top-bar viewer indicator: "Viewing live: [canvas title] — [artist display name]."
  - All canvas input handlers: skip mutation paths if `is_viewer()`. Defense-in-depth even though the UI shouldn't expose them.
- Viewers CAN: pan, zoom, scroll, toggle reader mode, click reader-mode branch buttons, see other viewers/collaborators in the peer list, jump to other peers' cameras, chat.

**Effort:** 3–4 days. The new menu + dialog is trivial. The viewer-mode UI gating is a sweep across `Toolbar`, `DesktopDrawingProgramScreen`, `PhoneDrawingProgramScreen`, and the few mouse/key handlers — small per-site changes, but spread across multiple files.

**Output:** a subscriber can paste a token + lobby address, connect, and watch the artist work live in a clean read-only viewer.

## 5. Handshake protocol changes (concrete)

Today (`World.cpp:171`):

```
Client → Server : SERVER_INITIAL_DATA { display_name }
Server → Client : CLIENT_INITIAL_DATA { file_display_name, client_data_obj_id, full_graph_snapshot }
```

Phase 0:

```
Client → Server : SERVER_INITIAL_DATA { display_name, optional<token_string> }

Server side, on receiving SERVER_INITIAL_DATA:
  if host is in subscriber-only mode:
    if !token_string.has_value(): reject + close
    (signature_bytes, payload_bytes) = split_token(token_string)
    payload = json_decode(payload_bytes)

    # The five-check verification — all must pass.
    if !ed25519_verify(payload.a, signature_bytes, payload_bytes): reject + close   # 1. signature
    if payload.a != host.own_member_pubkey:                          reject + close   # 2. identity binding
    if payload.k != host.own_app_pubkey:                             reject + close   # 3. app binding
    if payload.c != host.canvas_id:                                  reject + close   # 4. canvas binding
    if payload.e is not None and payload.e < now():                  reject + close   # 5. expiry

    is_viewer = true
  else:
    is_viewer = false  # existing collab behavior

Server → Client : CLIENT_INITIAL_DATA { file_display_name, client_data_obj_id, full_graph_snapshot }
                  # ClientData for this new client has is_viewer flag set
```

The host needs **no external lookup** to verify a token: `payload.a` is the verification key, the host already knows its own member pubkey (from portal credentials), its own app pubkey (locally generated and registered), and its own canvas_id (from the open `.inkternity` file). Self-contained.

The `ClientData` `NetObj` gets one new boolean field `is_viewer` (default false). Backwards-compatible: pre-Phase-0 clients deserialize false, get full collab access (which is what they had).

Token format on the wire (URL-safe, ~160 bytes):

```
<base64url(64-byte ed25519 signature)> "." <base64url(json_payload)>
```

JSON payload kept minimal (single-letter keys to fit in compact tokens):

```json
{"a":"GAB...","k":"abcd...","c":"4f9e...","i":1730150400,"e":1761686400,"sub":"7a3f..."}
```

Field reference:

| Field | Meaning |
|---|---|
| `a` | Artist member Stellar pubkey (the signer; on-chain in `hvym_roster`) |
| `k` | Artist Inkternity app pubkey (the install this token grants access to) |
| `c` | Canvas UUID |
| `i` | Issued-at unix timestamp |
| `e` | Expires-at unix timestamp (omit for forever) |
| `sub` | sha256 of subscriber email (audit trail; not currently checked at verify time) |

No nesting, no claims beyond what Phase 0 needs. We can extend with optional fields (subscriber Stellar pubkey for stronger binding, tier, scopes) later without breaking existing tokens.

## 6. The artist's full Phase-0 flow

```
ONE-TIME SETUP (per Inkternity install):
  1. Get portal-issued Inkternity credentials via /launch (existing Metavinci pattern,
     extended with an Inkternity entry in apps.json).
  2. Open Inkternity, paste credentials. App generates a fresh ed25519 app keypair
     and stores it locally encrypted alongside the credentials.
  3. From the lobby (file-select screen, not inside a canvas):
     Settings → HEAVYMETA → Inkternity App Key → "Copy public key".
  4. On the portal: /dashboard/inkternity/apps → "Register new" → paste the pubkey,
     give it a label ("Studio iMac"). One-time per machine.

ON THE PORTAL (one-time per canvas):
  5. /dashboard/inkternity → "Add canvas" → enter title, description, price USD,
     pick which registered install will host this canvas (default: most recent).
  6. Portal creates canvas record + Stripe Connect product. Returns canvas_id + buy URL.

IN INKTERNITY (one-time per canvas):
  7. Open the .inkternity file on the install you selected in step 5.
  8. Menu → Publish for Subscribers... → app calls portal API, confirms its app
     pubkey is registered, stamps {canvas_id, artist_member_pubkey,
     app_pubkey_at_publish} into the file.
  9. Save.

EVERY TIME THE ARTIST WANTS TO BE LIVE:
  10. Open the published .inkternity file. Menu → Host. Standard host UX.
  11. Menu → Lobby Info → copy address. Post to Twitter/Discord/etc:
        "I'm live drawing! Paste this lobby address into your Inkternity:
         Yo3MhWqbn3...abc123XYZ"
  12. Subscribers join (next column). Artist draws. Disconnect when done.
```

## 7. The subscriber's full Phase-0 flow

```
ONE-TIME SETUP:
  1. Get portal-issued Inkternity credentials via /launch (existing Metavinci pattern,
     extended with an Inkternity entry in apps.json).
  2. Open Inkternity, paste credentials at first run, app validates against roster.

PER PURCHASE:
  3. Visit the artist's buy URL (shared via the artist's social channels).
  4. Stripe Checkout → pay → token shown in-browser, also emailed.
  5. Save the token somewhere (Phase 0 has no portal subscriber-side dashboard).

PER SESSION (when the artist is live):
  6. Artist posts "I'm live drawing! Lobby: Yo3MhWqbn3...abc123XYZ"
  7. In Inkternity: Menu → Connect (with token) → paste lobby address + token.
  8. Connection opens; you see the canvas. Top bar: "Viewing live: <canvas title>
     — <artist name>." Pan, zoom, follow the artist. Watch them work.
  9. Artist closes lobby; your viewer disconnects cleanly.
```

## 8. Risks and mitigations

- **Master Stellar secret decrypted on every token mint.** The portal briefly holds the artist's plaintext member secret in webhook handler memory between Banker+Guardian decrypt and signature emission, then discards. Same risk profile as today's `/launch` endpoint that issues Metavinci credentials — known and accepted in HEAVYMETA today, no NEW exposure introduced. **Phase 0.5 mitigation:** delegation cert model — artist's master key signs ONE cert per Inkternity install authorizing a portal-held `inkternity_token_signing_key`; subsequent token mints use the signing key alone, master never re-touched.
- **Token leak.** Anyone with the token string can join the corresponding artist's canvas on the corresponding install. Mitigation: tokens are tied to (canvas, artist_install) pair, not transferable. For high-value canvases, use short `expires_at`. Phase 1 adds subscriber-stellar-pubkey binding (`payload.sub_pubkey` + a challenge-response signing step at handshake) so token + holder-signature are required.
- **Artist app key compromised.** Mitigation: artist re-registers a new app pubkey on the portal. New tokens will carry the new `k`; the new install's `own_app_pubkey` matches; the compromised install's pubkey is now stale and any leaked tokens addressed to it cannot be replayed against the new install. No on-chain action required, no migration of past sales — subscribers' tokens for canvases published from the old install must be re-issued by the portal (one-line script: re-sign all unexpired tokens with new `k`).
- **Subscriber loses their token email.** Phase 0 has no resend mechanism on the portal. Mitigation: portal stores tokens server-side; add `/dashboard/inkternity/my-tokens` as a fast follow.
- **Artist's lobby address rotates per session.** Subscriber needs the latest address. This is the "no new sharing mechanism" decision biting. Mitigation: artist routinely re-posts. Phase 0.5 can add "portal publishes artist's current lobby" as an opt-in surface (see §A.2).
- **Viewer-mode UI gaps.** Some keystrokes might still trigger mutations if we miss a handler. Mitigation: defense-in-depth at NetObj update layer in Phase 1; Phase 0 ships with UI-only gating + manual test pass.
- **Signaling server downtime.** HEAVYMETA-run signaling is now the single point of failure for sessions. Mitigation: monitoring + auto-restart on the VPS. Multi-region is a Phase 2+ concern.
- **infinipaint.com endpoints in old binaries.** Older Inkternity installs still hit infinipaint infrastructure. Mitigation: Phase 0 is a new fork release; communicate the cutover; maintain config compatibility so users can manually switch back if needed.
- **Format-version bump.** TRANSITIONS bumped to 0.10; Phase 0 distribution bumps to 0.11 → 0.12. Confirm at implementation time and adjust the load-path version gates accordingly.

## 9. Subtask breakdown

### A. Infrastructure

| | Deliverable |
|---|---|
| P0-A1 | Provision VPS, configure DNS for `signal.heavymeta.art` and `turn.heavymeta.art`. |
| P0-A2 | Deploy `coturn` with credentials, Let's Encrypt cert for the WSS signaling endpoint. |
| P0-A3 | Deploy `libdatachannel`'s stock signaling server (Python or Rust reference impl), smoke test via local Inkternity build. |
| P0-A4 | Update `assets/data/config/default_p2p.json` to point to HEAVYMETA endpoints. Build + smoke test. |

### B. Portal

| | Deliverable |
|---|---|
| P0-B1 | Schema: `inkternity_app_keys` + `inkternity_canvases` + `inkternity_tokens` tables. |
| P0-B2 | Token signing helper: takes (artist_user_id, payload) → decrypts member secret via Banker+Guardian, signs, discards plaintext, returns wire-format token. Mirrors the existing `/launch` decrypt-sign-discard pattern. |
| P0-B3 | Artist `/dashboard/inkternity/apps` — list, register, label app pubkeys. |
| P0-B4 | Artist `/dashboard/inkternity` — list, add, edit canvases (each picks a registered app pubkey as the host). Stripe Connect product creation. |
| P0-B5 | Public `/inkternity/canvas/{canvas_id}` buy page + Stripe Checkout. |
| P0-B6 | Stripe webhook: on payment success, mint token via P0-B2, persist, email. |
| P0-B7 | API endpoints Inkternity calls: `POST /api/inkternity/canvases`, `GET /api/inkternity/apps/{app_pubkey}` (registration check). |

### C. Inkternity — artist side

| | Deliverable |
|---|---|
| P0-C1 | App keypair generation on first run after credentials load; encrypted local store. **Lobby** (FileSelectScreen) settings panel exposes the pubkey + Copy button under HEAVYMETA → Inkternity App Key. (Per-install identity belongs at lobby scope, not per-canvas.) |
| P0-C2 | Format bump 0.11 → 0.12 with `canvas_id` + `artist_member_pubkey` + `app_pubkey_at_publish` fields. Save/load with version gate. |
| P0-C3 | Credential store: read portal-issued Inkternity credentials at startup. |
| P0-C4 | `Menu → Publish for Subscribers...` UI + portal API call. Confirms app pubkey is registered; stamps canvas_id + member pubkey + app pubkey into file. |
| P0-C5 | Subscriber-only host-mode detection (canvas has canvas_id + artist is hosting). |
| P0-C6 | Token verification helper: ed25519 verify against `payload.a`, plus the four binding checks (identity, app, canvas, expiry). No bundled portal pubkey. |
| P0-C7 | Extend `SERVER_INITIAL_DATA` handshake to receive token; reject-on-failure path. |
| P0-C8 | Set `ClientData::is_viewer=true` for accepted-via-token connections. |
| P0-C9 | Peer-list indicator: distinguish viewers from collaborators. |

### D. Inkternity — subscriber side

| | Deliverable |
|---|---|
| P0-D1 | `Menu → Connect (with token)...` dialog. Two fields, single Connect button. |
| P0-D2 | Pass token through `World::init_client` → handshake. |
| P0-D3 | Connection-rejected error handling: clean close, user-facing toast. |
| P0-D4 | `ClientData::is_viewer` field + accessor. Network-synced, default false. |
| P0-D5 | Viewer-mode UI gating: hide toolbox, hide mutating menu items. |
| P0-D6 | Top-bar viewer indicator with canvas title + artist name. |
| P0-D7 | Defense-in-depth: input handlers no-op when `own_client_data.is_viewer()`. |

### E. Manual test pass

| | Deliverable |
|---|---|
| P0-E1 | End-to-end: artist publishes canvas, subscriber buys, artist hosts, subscriber joins, viewer mode confirmed, both disconnect. |
| P0-E2 | Negative: bad token rejected, expired token rejected, mismatched-canvas token rejected. |
| P0-E3 | Mixed lobby: artist hosts non-published canvas — existing collaborator flow still works (no token check, full edit access). |
| P0-E4 | Resilience: artist disconnects mid-session, viewers see clean disconnect. |
| P0-E5 | Infrastructure: full session over HEAVYMETA signaling + TURN, no fallback to infinipaint endpoints. |

## 10. What Phase 0 explicitly defers to `DISTRIBUTION.md`

- **Per-subscriber encryption** — Phase 0 sends the same canvas state to every viewer. Anyone with intercepted packets sees plaintext.
- **IPFS / Pintheon pinning** — no out-of-band asset storage. Skins still local-only as today.
- **Recurring subscriptions** — every renewal is a fresh token purchase.
- **Replay / past Issues** — can't view a canvas after the artist disconnects.
- **Per-tier branching narratives** — viewers all see the same canvas; no gated reader-mode branches.
- **Subscriber-local annotations** — no.
- **Time-locked reveals** — no, beyond the simple `expires_at`.
- **Subscriber discovery surface** — no "my subscriptions" page in the portal yet.
- **Andromica/Pintheon integration** — Inkternity Phase 0 does not deploy IPFS tokens or interact with `hvym_collective` at all.

The promise of Phase 0: *every line of code we write here is reusable in the full design*. The handshake extension supports any future token format. The viewer mode is the same viewer mode `DISTRIBUTION.md` will use. The portal canvas/token tables become the natural foundation for recurring subs, IPFS-backed Issues, etc.

## 11. Open questions before starting

- **What's the Stripe Connect application-fee policy for Inkternity sales?** Match commissions (0% paid tier, 5% free)? Or new tier?
- **Branding: what does the artist's buy page look like?** Inherits portal styling? Custom per-canvas hero image? Phase 0: minimal; the artist's title + description + price + Pay button is enough.
- **Does HEAVYMETA already have a VPS we should reuse?** hvym_tunnler is deployed at `tunnel.hvym.link`. If that VPS has spare capacity, signaling + coturn can co-locate cheaply. If not, fresh small VPS is fine.
- **App-keypair re-derivation if user reinstalls?** First run after a clean install regenerates a fresh app pubkey, which the artist must re-register on the portal (and re-issue tokens for active subscribers). Acceptable for Phase 0; document clearly in artist onboarding. Phase 0.5 could store the app keypair in the portal-encrypted credential bundle so reinstalls preserve it.
