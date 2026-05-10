# Inkternity — Artist-First Distribution Model

> **Audience:** the agent (and any human contributor) working inside the Inkternity repo, plus anyone reviewing the proposal who knows HEAVYMETA's broader stack.
>
> **Goal of this doc:** define how Inkternity becomes a sovereign 1-to-1 publication channel inside the HEAVYMETA cooperative — distinct from its existing live-collaboration networking, bundled as a tier-gated entitlement, and designed so artists own the artist↔subscriber relationship end-to-end.

## 1. Product summary

Inkternity today is a single-artist or small-trusted-group canvas tool with a P2P live-collab layer (WebRTC, WebSocket signaling, server-authoritative `NetObj` replication, ephemeral display-name identity). It saves to a local `.inkternity` file. Reader mode (PHASE1 §7, TRANSITIONS.md) gives the artist a curated camera-path consumption surface — waypoints, transitions, branches.

This doc proposes a **second, parallel networking mode** — *Distribution* — sitting alongside the existing *Collab* mode, and aligning with HEAVYMETA's "bespoke one-to-one distribution" thesis (`hvym-market-muscle/HEAVYMETA_THESIS.md` §5.1):

> "every piece of content is uniquely encrypted for each subscriber using per-subscriber ECDH shared secrets derived from Ed25519/X25519 Stellar keypairs. There is no 'master copy' to scrape, no shared CDN endpoint to intercept, and no platform intermediary that controls the relationship."

**The product proposition for an Inkternity-publishing artist:**

> *"You don't sell a file. You don't post on a platform. You publish a living canvas. Each subscriber gets their own cryptographically distinct copy, bound to their wallet, that updates as you draw. They read it through reader mode — your curated camera path, your transitions, your branches. They don't get a downloadable bitmap. They get a running thread of your work, sovereign to the relationship between you and them."*

Three things this is **not**:

- **Not a marketplace.** No discovery layer. Discovery happens on the HEAVYMETA portal (`/commission/{moniker_slug}`) and through the artist's own channels. Inkternity is the conduit, not the storefront.
- **Not a CDN.** There is no shared "popular copy." Every subscriber's payload is encrypted to that subscriber's pubkey. Leak-resistant by design.
- **Not a replacement for Collab mode.** Live multi-user editing (the existing WebRTC P2P) is a separate axis: artist-with-collaborators. Distribution mode is artist-to-subscribers.

## 2. Inheritance from existing systems

### 2.1 Inkternity already has

- **Generic per-object replication** — `NetObjManager::register_class<T>` (NetObjManager.hpp:82) with `writeConstructorFunc` / `readConstructorFunc` / `readUpdateFunc`. Used by `Waypoint`, `Edge`, `CanvasComponentContainer`, `DrawingProgramLayerListItem`, `WorldGrid`, `ClientData`. Generic enough to carry a "publish this snapshot" message.
- **Server-authoritative initial-state transfer** — `CLIENT_INITIAL_DATA` in World.cpp:446–452 packs the entire graph state for a joining peer. Same wire format we want for "deliver canvas snapshot to subscriber."
- **Separate file save/load and network sync paths** — they share the cereal binary format but the routes don't intersect. We can add a third route ("distribution payload write") without disturbing either.
- **Reader mode as a consumption surface** — waypoints + transitions + branches already define a curated read experience. Subscribers experience the comic *inside* reader mode, not as a downloadable image.
- **Format-version gated load paths** — INFPNT header → VersionNumber, with `if (version >= ...)` per field block. Same gating works for distribution-only fields.

### 2.2 Inkternity does NOT have

- **Persistent identity.** Display names are ephemeral session strings. No public-key binding, no signing.
- **Content addressing.** `NetObjID` is runtime-random; file edges reference nodes by positional index. There's nothing today that says "this canvas is content-hash X."
- **Selective replication.** Server today pushes every `NetObj` update to every connected client. There's no "send this layer only to subscriber Y."
- **Authenticated messages.** Plain WebSocket; no signature verification at the protocol level.
- **Out-of-band asset distribution.** Skins are deliberately local-only ("large payloads through NetObj messages are their own scoping problem and this fork is single-user anyway" — Waypoint.hpp:66–70). Distribution mode needs a real story for big payloads (canvas tiles, vector strokes, raster ink layers).

## 3. The HEAVYMETA framework Inkternity plugs into

These are concrete, shipped surfaces (citations from the research agents):

### 3.1 Identity layer — `hvym-stellar`

- Each cooperative member receives a Stellar Ed25519 keypair at portal enrollment. Stored encrypted (dual-key ECDH: Banker + Guardian). Exposed via the `hvym-stellar` library (Rust + Python).
- "**The person is not the key.**" The keypair is an *application credential*, not a user identifier. Stellar address never surfaces as a username (`RESEARCH-identity-and-application-credentials.md` §10).
- Membership state lives on-chain in the `hvym_roster` Soroban contract (mainnet ID `CBUS33CAIMTV7T4M4G3FTH35QBAY6VWY3K4IZTYTRPD45ZDSQMSIZ2AB`).

### 3.2 Entitlement layer — Portal-issued shared-key tokens

- The Collective Portal's `/launch` route issues `StellarSharedKeyTokenBuilder` tokens with caveats (network, tier, expiry, etc.). Two existing token kinds: Pintheon node creds, Metavinci desktop creds. Pattern: `generate_metavinci_credentials()` in `heavymeta_collective/launch.py`.
- **The token + secret-key pair is what activates a desktop app.** Token shown in-browser, secret emailed.
- Tier ladder (`heavymeta_collective/static/tiers.json`): `free` (0) → `spark` (1) → `forge` (2) → `founding_forge` (3) → `anvil` (4). Carries integer counts (`tunnels`, `cards_included`) and feature flags.
- Portal API for entitlement checks: `GET /member/{id}/tier`, `GET /tiers`, bearer-token auth via `BOT_API_TOKEN` (`hvym-market-muscle/telegram_bot/handlers/gating.py` is the canonical consumer pattern).

### 3.3 Distribution layer — IPFS + Lepus + per-subscriber encryption

- **Pintheon** = artist-owned IPFS node, exposes Kubo HTTP API on `localhost:5001`, gateway on `:8081`. The "lowest-friction integration is `Export to Pintheon` via the standard IPFS HTTP API" (`RESEARCH-integration-opportunities.md`).
- **HVYM Tunnler** exposes the artist's local Pintheon at `https://<stellar_address>.tunnel.heavymeta.art`.
- **Freenet-Lepus** stores ~2KB per-subscriber metadata datapods using Commitment-Weighted Persistence. One WASM contract code, N instances — **one per (creator, subscriber) pair**. Parameterized by both pubkeys, encrypted with their ECDH shared secret.
- **NINJS** (NewsML-G2) is the metadata schema for both Andromica galleries and Lepus datapod payloads. Inkternity comics produce NINJS payloads too.
- **Per-file IPFS tokens** are minted via `hvym_collective.deploy_ipfs_token()` for each published artifact.

### 3.4 What's missing in HEAVYMETA today (gaps Inkternity must address)

- **No recurring "subscribe to artist X" primitive on the portal.** Today it's one-time Stripe Connect commissions only. Membership dues are paid to the cooperative, not to artists.
- **No `pintheon-client` SDK yet** — the Python wrapper around IPFS HTTP API + IPTC + NINJS. We'll integrate against the raw IPFS HTTP API and contribute back to the SDK as it firms up.
- **No `inkternity_seats` (or analogous) entry in `tiers.json`.** We propose this.
- **No `generate_inkternity_credentials()` in `launch.py`.** We propose this.

## 4. Two-mode topology

Inkternity supports two networking modes, selectable per-canvas. They do NOT share the same wire protocol or trust model.

| | **Collab mode** (existing) | **Distribution mode** (new) |
|---|---|---|
| Trust | Mutually trusted peers | Artist-authored, subscriber-consumed |
| Topology | WebRTC P2P, WebSocket signaling, server-authoritative `NetObj` | One artist publishes; many subscribers pull |
| Replication | Live, all peers see all changes | Per-subscriber encrypted snapshots + deltas |
| Identity | Ephemeral display names | Stellar pubkeys, on-chain roster |
| Auth | None | Macaroon/Biscuit token scoped to subscriber |
| Payload | Whatever's in the `NetObj` | Subset of canvas (artist chooses what's published) |
| Storage | Live in memory + local `.inkternity` save | IPFS + per-subscriber Lepus datapod |
| Read surface | Editor + reader mode | Reader mode only (no editing) |
| Use case | Co-drawing with friends | Selling/sharing finished work to fans |

A canvas can transition Collab → Distribution (publish a snapshot) but not the reverse. Subscribers viewing in Distribution mode have a strictly read-only client.

## 5. Data model

### 5.1 Publishable unit: the *Issue*

A canvas snapshot intended for a subscriber. One canvas can have many Issues over time (Issue 1, Issue 2, ... — like comic issues).

```cpp
// New on-disk + on-IPFS structure (sketch)
struct Issue {
    std::string  title;             // "Chapter 4: The Diamond Path"
    uint64_t     issueNumber;       // monotonic per-canvas
    std::string  contentCID;        // IPFS CID of the encrypted .inkternity payload
    std::string  ninjsCID;          // IPFS CID of the NINJS metadata sidecar
    uint64_t     timestamp;         // Unix seconds, when artist published
    std::string  artistStellarAddr; // who signed this
    std::string  signatureEd25519;  // signature over (issueNumber, contentCID, timestamp)
};
```

Issues are immutable. Updates = new Issues. Subscribers see a chronological feed.

### 5.2 What's IN the published payload

The artist chooses, per-Issue, what to publish. Defaults:

- **Reader-mode canvas.** All canvas components VISIBLE in reader mode (sketch layers excluded — they're already hidden by Phase 2 layer kinds).
- **Waypoint graph.** Including transitions, branches, edge labels, speed/easing controls. The reader's curated experience IS the comic.
- **Skins.** Per-waypoint button images.
- **Strokes / vector data.** All inked content. Sketch layer raster data is excluded.

What's NOT published:
- Editor-only state (selection, tool config, undo history)
- Layer kinds marked SKETCH (raster scratch, design intent: hidden in reader mode)
- Per-canvas peer chat history from Collab mode
- The `.inkternity` save itself in raw form (subscribers receive the encrypted payload, not the source)

### 5.3 Per-subscriber encryption envelope

For each (issue, subscriber) pair:

```
plaintext  = serialized Issue payload (reader-mode subset of canvas, NINJS metadata)
key        = ECDH(artist_stellar_x25519_priv, subscriber_stellar_x25519_pub)
ciphertext = AES-256-GCM(key, plaintext)  // or ChaCha20-Poly1305
envelope   = {
    issue_number,
    artist_stellar_pub,
    subscriber_stellar_pub,
    ciphertext,
    signature_ed25519,    // artist signs the whole envelope
}
```

The envelope is what gets pinned to IPFS and referenced by CID inside the subscriber's Lepus datapod. **There is no shared file**. The artist runs the ECDH derivation N times and produces N envelopes per Issue (one per subscriber). This is the "bespoke one-to-one" architecture, applied to comic publication.

### 5.4 Subscriber datapod schema

Stored in Lepus, ~2KB per subscriber per artist:

```json
{
  "schema": "ninjs/2.0",
  "creator": "GAB...artist_stellar_addr",
  "subscriber": "GCD...subscriber_stellar_addr",
  "tier": "spark",
  "subscribed_at": 1730150400,
  "expires_at": 1761686400,
  "feed": [
    { "issue": 1, "envelope_cid": "Qm...", "published_at": 1730150500 },
    { "issue": 2, "envelope_cid": "Qm...", "published_at": 1730237000 }
  ]
}
```

Lepus's Commitment-Weighted Persistence ensures these datapods stay alive on the network as long as creator + subscriber jointly commit XLM to keep them.

## 6. Subscription as a primitive (the new HEAVYMETA piece)

Today the portal has no recurring "subscribe to artist X" surface. Inkternity is the place to introduce it. Two paths, presented as a UX choice for the artist:

### 6.1 Per-Issue purchase (lighter, ships first)

- Artist publishes Issue N.
- Portal `/inkternity/issue/{artist_slug}/{issue_number}` is a public landing page.
- Subscriber pays one-time Stripe Connect amount (artist sets the price per Issue, or per Issue range).
- Portal generates a per-subscriber envelope, pins it, adds it to the subscriber's datapod.
- Subscriber opens Inkternity (or a future thin reader) and the new Issue appears in their feed.

This **slots into the existing portal commissions infrastructure** (Stripe Connect Express, artist-as-merchant-of-record, 0%/5% platform fee by tier). Lowest implementation cost.

### 6.2 Recurring subscription (the innovation)

- Artist offers a subscription: "$X/month for all my Inkternity canvases" or "$Y/month for this canvas series."
- Subscriber pays via Stripe Connect recurring (or XLM streaming via Stellar's claimable balances / SAC).
- Subscriber's datapod is auto-extended each renewal cycle.
- Artist can publish at any cadence; subscribers automatically receive each Issue's envelope as it's pinned.
- **If subscription lapses:** new Issues are not encrypted to that subscriber. Past Issues remain decryptable forever (the subscriber's wallet still has the ECDH derivation; the past envelopes are still on IPFS). This is critical: subscriber owns their archive, not access-revocable.

This adds a new portal table (`subscriptions`), a new portal route (`/dashboard/subscriptions`), a new entry in `apps.json` for Inkternity (separate from the Metavinci binary), and a Stripe Connect recurring-billing handler.

### 6.3 The artist-author flow

In Inkternity, with a finished canvas:

```
File menu → Publish Issue...
  ┌─────────────────────────────────────────────┐
  │ Publish Issue 4: "The Diamond Path"        │
  │                                             │
  │ Subscribers (loaded from Portal):           │
  │   ☑ Spark+ tier (12 subscribers)            │
  │   ☐ Forge+ tier only (3 subscribers)        │
  │   ☐ Specific list...                        │
  │                                             │
  │ Include:                                    │
  │   ☑ All ink + color layers                  │
  │   ☑ Waypoints + transitions                 │
  │   ☑ Skins                                   │
  │   ☐ Sketch layers (default off)             │
  │                                             │
  │              [Cancel]  [Publish]            │
  └─────────────────────────────────────────────┘
```

On Publish:
1. Inkternity strips the canvas to the published-subset.
2. For each subscriber: derive ECDH shared secret, encrypt payload, sign envelope.
3. Push envelopes to the artist's local Pintheon node (IPFS HTTP API at `localhost:5001`).
4. For each subscriber: update their Lepus datapod with the new envelope CID.
5. Mint a per-issue IPFS token via `hvym_collective.deploy_ipfs_token()` (provenance, OPUS reward).
6. Show artist a published-receipt: count, token ID, IPFS CIDs.

Artists with hundreds of subscribers see an N-second progress bar; encryption is fast (single-digit-ms per subscriber for typical canvas sizes).

### 6.4 The subscriber-read flow

Subscriber opens Inkternity (with credentials previously obtained from `/launch`) and sees:

```
┌──────────────────────────────────────┐
│ Subscriptions                        │
│                                      │
│ ▾ Artist: jane.heavymeta.art         │
│   • Issue 1: "Origin"   (read)       │
│   • Issue 2: "Crossing" (read)       │
│   • Issue 3: "Echo"     (NEW)        │
│                                      │
│ ▾ Artist: marco.heavymeta.art        │
│   • Issue 1: "Pilot"    (NEW)        │
│                                      │
└──────────────────────────────────────┘
```

Click an Issue → Inkternity:
1. Fetches the envelope CID from the subscriber's datapod.
2. Pulls envelope bytes from IPFS (any reachable Pintheon, gateway, or the artist's tunnel).
3. ECDH-derives the key, decrypts in-memory.
4. Loads the canvas into a **read-only viewer** that's reader mode without editor chrome.
5. Subscriber experiences the curated waypoint walk.

The decrypted bytes never hit disk in plaintext form (caveat: simple v1 may cache them; harden in v2).

## 7. Innovation surfaces (what makes 1-to-1 *interesting*, not just secure)

These are the features that make the model feel novel, not just "DRM but blockchain."

### 7.1 Living canvas — published Issues are *thread updates*

A subscriber doesn't buy a static file; they subscribe to an evolving canvas. Issue 5 might be Issue 4 plus 200 new strokes and a new waypoint branch — not a clean cut. The subscriber's reader view "grows" each time the artist publishes. Reader mode's existing waypoint navigation handles this naturally: new waypoints appear as new chapter entries.

### 7.2 Per-tier branching narratives

The artist can encode different paths for different subscriber tiers using the existing branch/transition machinery. A waypoint that branches: edge A is unlocked for free-tier, edge B for spark+, edge C for anvil-only. The reader-mode branch overlay only shows edges the subscriber's tier qualifies them for. **The tier ladder becomes a narrative tool**, not just a billing knob.

Implementation: extend `Edge` with an optional `min_tier` field (defaults `free`), gated in `outgoing_choices()` by checking the subscriber's tier mirror.

### 7.3 Subscriber-local annotations

Subscribers can leave annotations (notes, marks) on their personal copy. These are stored in their own local datapod, encrypted to themselves, never seen by other subscribers. Optionally shareable with the artist (signed payload back to artist's pod).

This is genuinely novel for a "publication" model: a private artist↔subscriber commentary thread, integrated with the work, sovereign to the relationship.

### 7.4 Time-locked reveals

The artist publishes Issue 5 with a `reveal_after: 2026-12-25` flag. Subscriber's local Inkternity refuses to decrypt before that timestamp (envelope still arrives, key derivation gates on local clock + signed timestamp from the artist). Useful for synchronized event-style releases inside an ongoing series.

### 7.5 Leak-resistance is *visible* to subscribers

Every published canvas tile shows a faint watermark of the subscriber's Stellar address while in reader mode (artist-toggleable). Combined with per-subscriber encryption, the message is clear: a leak traces. This is a UX of trust, not a deterrent — subscribers see they're being treated as one-of-one.

## 8. Tier-aware feature gating inside Inkternity

Beyond who-can-receive-an-Issue, the desktop app itself gates features by the artist's *own* tier (read from the local roster mirror):

| Feature | Gated by |
|---|---|
| Open canvas, draw, save locally | Free (no entitlement check) |
| Collab mode (existing P2P) | Free |
| **Publish Issue** (1 active canvas) | Spark+ |
| Publish Issue, multiple active canvases | Forge+ |
| Per-tier branching narrative | Forge+ |
| Recurring subscription (vs per-Issue) | Founding Forge+ |
| Custom Tunnler subdomain for distribution | Anvil |

This mirrors `hvym-market-muscle/telegram_bot/handlers/gating.py`'s numeric-ladder pattern. Tier integers: `free=0 → anvil=4`, `user_tier_level >= required_level`.

## 9. Architecture map (where each piece lives)

```
                                 ┌─────────────────────────┐
                                 │  Heavymeta Collective   │
                                 │       Portal            │
                                 │                         │
                                 │  • /launch              │ ◄── issues Inkternity creds
                                 │  • /dashboard           │     (Stellar shared-key token,
                                 │     /subscriptions      │      caveats: tier, expiry, scope)
                                 │  • Stripe Connect       │
                                 │  • hvym_roster polling  │
                                 └────────────┬────────────┘
                                              │
                                              │  reads/writes
                                              ▼
                  ┌──────────────────────────────────────────────────┐
                  │       hvym_roster + hvym_collective              │
                  │       (Soroban, Stellar mainnet)                 │
                  └──────────────────────────────────────────────────┘
                                              ▲
                                              │  local mirror
                                              │
              ┌───────────────────────────────┼──────────────────────────────┐
              │                               │                              │
              ▼                               ▼                              ▼
   ┌──────────────────┐         ┌────────────────────────┐       ┌──────────────────┐
   │   Inkternity     │         │     Inkternity         │       │   Inkternity     │
   │   (artist)       │         │     (subscriber)       │       │   (subscriber)   │
   │                  │         │                        │       │                  │
   │  • Edits canvas  │         │  • Reads canvas        │       │  • Reads canvas  │
   │  • Publishes     │         │    (read-only)         │       │    (read-only)   │
   │    Issue         │         │  • Local annotations   │       │                  │
   │  • Per-sub       │         │                        │       │                  │
   │    ECDH encrypt  │         │                        │       │                  │
   └────────┬─────────┘         └──────────┬─────────────┘       └────────┬─────────┘
            │                              │                              │
            │ Pintheon HTTP                │ IPFS pull                    │ IPFS pull
            │ (localhost:5001)             │ (any gateway / tunnel)       │
            ▼                              ▼                              ▼
   ┌──────────────────────────────────────────────────────────────────────────────┐
   │                              IPFS network                                     │
   │  (Kubo + Pintheon pinning + HVYM Pinner replication)                         │
   │                                                                               │
   │   • Per-Issue per-subscriber envelopes (one CID per (issue, subscriber))     │
   │   • NINJS metadata sidecars                                                   │
   └──────────────────────────────────────────────────────────────────────────────┘
            │                                                              │
            │ datapod read/write                                           │
            ▼                                                              ▼
   ┌──────────────────────────────────────────────────────────────────────────────┐
   │                           Freenet-Lepus                                       │
   │  (per-subscriber datapods, ~2KB each, CWP-persisted)                         │
   │                                                                               │
   │   • Issue feed (CIDs of envelopes)                                            │
   │   • Subscription metadata (tier, renewal, expiry)                             │
   └──────────────────────────────────────────────────────────────────────────────┘
```

## 10. Risks

- **Encryption performance at scale.** Artist with 1,000 subscribers means 1,000 ECDH derivations + 1,000 AES encrypts per Issue. Modern hardware handles this in single-digit seconds for typical canvas sizes; published-payload size is the dominant factor. **Mitigation:** background-thread the encryption and incremental Issue support (only encrypt the *delta* over the prior Issue, with hash-chain verification, when subscribers have all prior Issues).

- **Subscriber list staleness.** When does Inkternity re-sync the subscriber list from the portal? Per-publish? Periodically? **Mitigation:** sync on publish (always fresh), cache locally, expose "last synced N minutes ago" in the publish dialog.

- **Lost subscriber wallet = lost archive.** If a subscriber loses their Stellar secret key, all past Issue envelopes become un-decryptable. **Mitigation:** the portal's existing key-recovery (Banker + Guardian dual-key) extends to this case — the recovered key still derives the same ECDH secret. Document explicitly.

- **Artist key compromise.** If an artist's key leaks, anyone can decrypt new envelopes addressed to them (since ECDH is symmetric). **Mitigation:** key rotation via the portal, with on-chain notice; new Issues use the new key, old Issues unaffected (subscriber's archive remains decryptable with the old key).

- **Subscriber-local plaintext caching.** First-pass implementation may cache decrypted Issue bytes for performance. **Mitigation:** v1 ships with explicit "Forget cached Issues" action; v2 keeps everything in memory only.

- **The portal doesn't yet have a recurring-subscription primitive.** This proposal essentially introduces one. **Mitigation:** ship per-Issue purchase first (slots into existing Stripe Connect) and add recurring as a portal milestone in parallel. Inkternity's distribution model works with either.

- **No `pintheon-client` SDK exists.** Implement against raw IPFS HTTP API (Kubo's `/api/v0/*`); contribute the Inkternity-side wrapper back as a candidate SDK seed.

- **`NetObj` framework was designed for trusted peers.** Distribution mode does NOT use it — this is a separate, signed-payload code path. Don't get tempted to "add signing to NetObj" — keep the two modes architecturally distinct.

- **What if a subscriber's tier downgrades mid-cycle?** Their datapod still has past envelopes (they own those forever). New Issues post-downgrade are gated by their current tier at publish time. Document this explicitly so artists know how their tier-gated branching behaves on subscriber demotion.

- **Reader mode wasn't designed for "incremental issue arrival."** New waypoints showing up between sessions might surprise users. **Mitigation:** "what's new in Issue N" highlight badge on newly-added waypoints in the tree-view, fades after first visit.

## 11. Subtask breakdown

This is a multi-phase build. Phase 3 covers the foundational pieces; Phase 4 lights up the recurring subscription and innovation surfaces.

### Phase 3a: Identity and credential plumbing (Inkternity-side)

| | Deliverable |
|---|---|
| D1 | Add `hvym-stellar` (or its C++/Python binding) as a dependency. Read Stellar keypair from local credential store at startup. |
| D2 | First-run flow: app prompts for portal-issued token+secret, validates against the on-chain `hvym_roster` (with local mirror), stores credentials. |
| D3 | Local roster mirror: SQLite cache of cooperative members + tiers, polled periodically from portal `/member/{id}/tier` and on-chain Soroban events. |
| D4 | Tier-gating helper: `inkternity::has_entitlement(feature)` checks current artist's tier against feature minimums. |

### Phase 3b: Publication primitives

| | Deliverable |
|---|---|
| D5 | `Issue` data model + serialization (separate from `.inkternity` save format, but reuses cereal). |
| D6 | "Reader-mode subset" extraction: serialize only the canvas pieces visible in reader mode + waypoint graph + skins. |
| D7 | ECDH envelope encryption: integrate `hvym-stellar` X25519 derivation, AES-256-GCM, signature. |
| D8 | Pintheon HTTP integration: POST envelopes + NINJS to local Kubo, get back CIDs. |
| D9 | Lepus datapod write: append new Issue feed entry per subscriber. |
| D10 | `Publish Issue` UI in Inkternity (artist side). Subscriber list pulled from portal. |

### Phase 3c: Subscription primitives (portal-side)

| | Deliverable |
|---|---|
| D11 | `apps.json` entry for Inkternity. |
| D12 | `tiers.json` extended with `inkternity_active_canvases` count + feature flags. |
| D13 | `generate_inkternity_credentials()` in `launch.py`. |
| D14 | Per-Issue purchase landing page `/inkternity/issue/{artist_slug}/{issue_number}` (Stripe Connect, one-time). |
| D15 | Recurring subscription table + `/dashboard/subscriptions` (Stripe Connect recurring) — gated to Founding Forge+ initially. |

### Phase 3d: Subscriber-read flow

| | Deliverable |
|---|---|
| D16 | `Subscriptions` panel in Inkternity (subscriber side): lists artists, Issues, NEW badges. |
| D17 | Read-only viewer mode: reader mode without editor chrome / publish UI / save-to-disk. |
| D18 | Decryption pipeline: pull envelope from IPFS, derive ECDH key, decrypt in-memory, load canvas. |
| D19 | "Forget cached Issues" action in subscriber UI. |

### Phase 4: Innovation surfaces

| | Deliverable |
|---|---|
| D20 | Per-tier branching: `Edge::min_tier` field, gated in `outgoing_choices()` by subscriber's tier mirror. |
| D21 | Subscriber-local annotation layer: encrypted to subscriber, optionally shareable to artist. |
| D22 | Time-locked Issues: `reveal_after` envelope field, local-clock + signed-timestamp gating. |
| D23 | Subscriber-address watermark in reader mode (toggleable). |
| D24 | Incremental Issues: hash-chained delta encryption to reduce per-Issue payload size at scale. |

D1–D10 are roughly **3–4 weeks** of focused work; D11–D15 require portal-side coordination (~2 weeks separate); D16–D19 are another ~2 weeks. D20+ is open-ended innovation backlog.

## 12. Out of scope

- **Discovery / marketplace.** Artists discover subscribers via their own channels (their `/commission/{moniker_slug}` page on the portal, social, etc.). Inkternity isn't a search engine.
- **Migration of existing `.inkternity` files** to the published format. Artists re-publish from scratch; no auto-conversion.
- **Multi-artist collaboration on a published Issue.** Distribution mode is single-author per Issue. (Co-authorship can happen in Collab mode and then one artist publishes.)
- **Rich subscriber→artist messaging.** Annotations are the only feedback channel in v1.
- **Mobile reader app.** Inkternity is desktop-only today; a thin reader-only mobile/PWA target is its own project.
- **Decentralized Identifier (DID) integration.** The HEAVYMETA-internal Stellar identity stack is sufficient and consistent with the rest of the cooperative.
- **Per-subscriber price discrimination at the artist's discretion.** Artists offer one price per tier-bucket, not per-individual-subscriber. (Per-subscriber pricing is a slippery UX; defer.)

---

## Appendix A: Where each existing Inkternity piece is reused

| Inkternity asset | Used by |
|---|---|
| `WaypointGraph` + `Waypoint` + `Edge` + transitions | The reader's curated read flow inside published Issues |
| Reader mode + branch overlay + back button | Subscriber's read surface (with chrome trimmed) |
| `.inkternity` cereal serialization | Source format for the encrypted Issue payload (subset) |
| `LayerKind::SKETCH` reader-mode hide | Used to exclude sketch layers from the published payload |
| `NetObjManager` framework | NOT reused for distribution; kept separate for Collab mode |
| `BezierEasing` + per-waypoint speed/easing | Carried into published Issues; subscribers experience the same pacing |
| Skin capture (`ButtonSelectTool`) | Carried into published Issues; subscribers see the artist's button art |

## Appendix B: Where this slots into HEAVYMETA's existing artifacts

| HEAVYMETA piece | How Inkternity uses it |
|---|---|
| `hvym_roster` Soroban contract | Source of truth for tier; polled into local mirror |
| `hvym_collective.deploy_ipfs_token()` | Mints per-Issue provenance token, OPUS reward |
| `StellarSharedKeyTokenBuilder` | Issues Inkternity desktop credentials at portal `/launch` |
| Pintheon (Kubo HTTP API) | Artist's local IPFS node for pinning envelopes |
| HVYM Tunnler | Artist's public distribution endpoint (`<addr>.tunnel.heavymeta.art`) |
| Freenet-Lepus | Per-(creator, subscriber) datapod with Issue feed |
| NINJS (NewsML-G2) | Issue metadata schema |
| `hvym-stellar` library | ECDH derivation, Ed25519 signing |
| Portal `/launch` `generate_*_credentials()` pattern | Mirrored for `generate_inkternity_credentials()` |
| Portal Stripe Connect | Per-Issue and recurring subscription billing |
| `tiers.json` | Inkternity's feature gating reads from this |
| `gating.py` numeric-ladder pattern | Mirrored in `inkternity::has_entitlement()` |

The thesis: Inkternity does NOT invent a parallel framework. It is a new conduit in HEAVYMETA's existing Creator/Conduit/Credential model, alongside Andromica.
