# Inkternity — Distribution Phase 1

> Scoping doc. Captures the **two-workstream design** for the
> distribution-phase work after the Phase 0 alpha
> (DISTRIBUTION-PHASE0.md). Verbally referred to in early planning as
> "Phase 3" because it lands chronologically after the Phase 1 / Phase 2
> *creative-tool* workstreams (PHASE1.md / PHASE2.md). The file name
> follows the canonical `DISTRIBUTION-PHASE{N}.md` series so the
> distribution-track docs stay grouped.

---

## 1. Product summary

Phase 0 shipped: HEAVYMETA-owned signaling + TURN, BUSL license,
cooperative-member viewer enforcement, cryptographic share-code
derivation (§12.5 of PHASE 0). The artist can host a canvas under a
stable share address; subscribers connect with portal-issued tokens.

This phase closes two functional gaps in the smallest way that works:

- **The artist's identity is fragile.** `app_secret` is generated on
  first run and stored locally. A reinstall throws it away, which
  invalidates every share code the artist has ever published. The §12.5
  cryptographic derivation is correct but resting on a substrate that
  can't survive a wiped machine.

- **Hosting requires actively clicking "Host" on a particular file.**
  The artist has to open each canvas they want to share, hit Host,
  and leave the file open. There's no notion of "this file is
  *published* — keep it served while Inkternity is open."

This phase adds, in the simplest possible form:

- **A.** Stellar-keypair encoding for the app keypair, plus a UI
  surface that shows the artist their address and lets them export
  their secret (raw `S...` and a BIP-39 mnemonic). Self-custodial
  end-to-end — no portal involvement, no cooperative backup, no
  funding handshake. Artist owns their keys, artist funds their own
  account from their own wallet if they want to receive payments.

- **B.** A "published" flag on canvas files. Files tagged published
  auto-start hosting in the background as soon as Inkternity launches,
  using the same `World::start_hosting` plumbing the Host button
  already drives. No new binary, no headless service, no platform
  daemon integration.

A **true always-on background service** — Inkternity running without
a window, serving canvases when the desktop app isn't even open — is
**explicitly tabled** for now (see §6). The tagged-file approach is
the 80% solution: subscribers can read as long as the artist's
machine is on with Inkternity launched, which covers most practical
publishing scenarios without the headless-daemon engineering cost.

---

## 2. Inheritance from Phase 0

What's already in place that Phase 1 builds on:

- `src/DevKeys.cpp` already generates a real ed25519 keypair on first
  run and persists it as hex in `inkternity_dev_keys.json`. The seed
  half is already a valid Stellar seed; we're changing the **encoding**
  more than the **entropy**.
- `include/Helpers/CanvasShareId.{hpp,cpp}` already derives share codes
  from `(app_secret, canvas_id)`. That derivation **does not change**
  in Phase 1 — the encoding migration has to preserve the first 32
  bytes of the secret byte-for-byte so every share code published
  before the migration keeps working after.
- `World::start_hosting` already runs the SUBSCRIPTION viewer-only
  enforcement (every connecting client → `isViewer=true`, host-side
  drop on writes per §12.2). Tagged-file auto-hosting just calls into
  this same code path at app-launch time, once per published file.
- Signaling server is stateless and shape-agnostic. Auto-publishing
  adds zero server-side surface.

What we don't yet have and Phase 1 must introduce:

- Stellar strkey encode/decode (`G...` / `S...`).
- BIP-39 mnemonic generation + derivation (SEP-0005 path `m/44'/148'/0'`).
- A Settings-tab UI surface that shows the address, reveals the
  secret/mnemonic on demand, and accepts an existing secret/mnemonic
  on a fresh install.
- A `published: true` flag in the canvas file format.
- An app-launch pass that auto-hosts every published file.
- A "this canvas is currently auto-published" affordance in the
  file-select view + Toolbar so the artist isn't surprised by a
  silent background host.

---

## 3. Workstream A — Stellar-keypair app identity (self-custodial)

### 3.1 Why Stellar specifically

Two reasons:

1. **The HEAVYMETA stack is already Stellar-native.** The portal,
   `hvym-stellar`, `dev_mint_token.py`'s G... pubkey support — they
   all assume Stellar strkey format. Encoding Inkternity's app keypair
   as Stellar aligns it with the rest of the cooperative's tooling
   instead of being a special case. Stellar's ed25519 == the
   underlying ed25519 we already generate; this is purely a
   **representation** change.

2. **Stellar is the cheapest viable rail for direct artist payments.**
   Once the artist funds the account themselves (1 XLM minimum
   reserve, paid from any wallet they already own — Lobstr, Solar,
   Freighter, an exchange withdrawal, etc.), they can receive XLM and
   USDC at the same address Inkternity uses for identity. Phase 1
   only needs to *enable* this by surfacing the address; settlement
   integrations come later.

### 3.2 Encoding migration

Today (`src/DevKeys.cpp::ensure_app_keypair`):

```
app_pub    = 64 hex chars   (32-byte ed25519 public key)
app_secret = 128 hex chars  (64 bytes: 32-byte seed || 32-byte pubkey)
```

After Phase 1:

```
app_pub      = "G..." (56 chars, strkey-encoded ed25519 pubkey,
                       CRC-16 checksum, type byte 0x30 << 3)
app_secret   = "S..." (56 chars, strkey-encoded ed25519 seed only,
                       CRC-16 checksum, type byte 0x12 << 3)
app_mnemonic = "word1 word2 ... word24"  (BIP-39, optional — present
                       if the user generated/imported via mnemonic)
```

Properties this keeps:

- The underlying ed25519 entropy is identical. The seed is the same
  32 bytes; we're just base32-encoding it with checksum + type byte.
- `CanvasShareId::derive_*` consumes the seed bytes, not the
  encoding. After migration the call site decodes `S...` → 32 bytes
  → unchanged HMAC. Every share code the artist published before the
  migration resolves identically after.

Properties this enables:

- `app_pub` is a real Stellar address. Once the artist funds it from
  their own wallet (no portal involvement, just a Stellar payment to
  the address), it's an on-network account that can receive XLM /
  USDC indefinitely.
- Backward compat: `DevKeys::load` detects format by field length
  (64/128 hex vs 56 strkey) and:
  - hex → migrate in place: decode to bytes, re-encode as strkey,
    rewrite the file once, log a USERINFO note.
  - strkey → use as-is.

### 3.3 Backup model — purely self-custodial

The artist owns their keys. The cooperative never sees the secret.
There is **no portal-mediated restore** in Phase 1.

**Critical UX constraint:** Inkternity's user base is crypto-averse
artists, not crypto power users. The crypto-identity surface stays
*invisible* by default — no first-run modal, no "back up your seed"
nag, no onboarding friction. Keys are generated silently the way
they are today; the artist who never touches Settings never sees a
mnemonic. Those who *do* want crypto functionality (receive payments,
move to a different machine, back up the recovery phrase) find it
behind an explicit Settings → Export affordance.

First-run flow:

1. `ensure_app_keypair` silently generates a fresh BIP-39 mnemonic (12
   words, 128 bits of entropy — see §8.2), derives the Stellar keypair
   via SEP-0005, writes all three fields (`app_pub` G..., `app_secret`
   S..., `app_mnemonic`) to `inkternity_dev_keys.json`. No UI. No
   modal. No artist-visible event.
2. The artist sees no difference from today — the file-select screen
   loads as normal.

Settings → Identity surface (when the artist eventually opens it):

- The existing G... pubkey copy field (already shipped in Phase 0).
- A new **"Export App Key"** button that opens a popup showing the
  S... secret + mnemonic with copy-to-clipboard buttons. Plain-text
  display, no hold-to-reveal theater — this matches the threat
  model: not a high-security wallet, just a recovery path for users
  who care.
- A symmetric **"Restore App Key"** button that opens a popup with
  a single paste field auto-detecting S... vs. 12/24-word BIP-39
  mnemonic. Destructive: re-derives the keypair from the input,
  overwrites `inkternity_dev_keys.json`. Gated on an explicit
  confirmation: *"This replaces your current identity. Share codes
  derived from your current keys will become unreachable until you
  restore them again."*
- (Optional) Horizon balance probe on Settings-open per §3.4.

Fresh-install / restore flow:

1. On startup, if `inkternity_dev_keys.json` doesn't exist, generate
   silently (default behavior). No prompts.
2. Restore is reachable only via the Settings → Identity → Restore
   App Key affordance described above — *not* at first launch (same
   crypto-averse principle).

Migration of existing hex-format installs:

The artist's machine may already hold an `inkternity_dev_keys.json`
with hex-format `app_pub` (64 chars) + `app_secret` (128 chars) from
the Phase 0 alpha. On load, `DevKeys` detects hex format, extracts
the existing 32-byte seed (the first half of the hex secret), re-encodes
it as `S...`, computes the matching G... pubkey, rewrites the file in
strkey form. The mnemonic field stays empty for these installs (we
never had a mnemonic associated with that random seed); Export shows
just the S... and a note. New installs get S... + mnemonic both;
migrated installs get S... only. No share codes break either way —
the seed bytes are preserved byte-for-byte.

That's it. No portal endpoints, no server-side state, no encrypted
bundles, no first-run friction. The user-experience cost is the
standard self-custodial warning (which we surface only when the
artist opens Export, not at launch); the engineering cost is one
BIP-39 implementation + one Export popup.

### 3.4 Self-funded on-ledger activation

If the artist wants to receive payments, they fund the account
themselves:

1. Settings → Identity shows the `G...` address with a "Copy" button
   and a QR code.
2. Artist sends ≥ 1 XLM from any external wallet (Lobstr, Solar,
   Freighter, a CEX withdrawal) to that address. Inkternity has no
   role here — it's just an address the artist controls.
3. Inkternity can poll the public Horizon endpoint for that account's
   existence + balance on Settings open, and surface a "Funded"
   indicator with the current balance. Polling is the only network
   call this whole workstream introduces.
4. No funding handshake, no payment intent, no cooperative subsidy
   path in Phase 1. The cooperative could *offer* subsidized funding
   as a portal perk later; out of scope here.

### 3.5 Surfaces touched

```
src/DevKeys.{hpp,cpp}            add strkey encode/decode + migration
                                 add BIP-39 generate + derive (SEP-0005)
                                 accept both hex and strkey on load
                                 expose app_pub_strkey(), app_secret_seed()

include/Helpers/CanvasShareId.cpp
                                 accept S... in addition to hex; decode
                                 strkey → 32-byte seed → unchanged HMAC

src/Subscription/TokenVerifier.cpp
                                 already accepts both G... and raw-hex
                                 per README; no change

src/Screens/FileSelectScreen.cpp Settings tab — Identity section:
                                 - show G... address + Copy + QR
                                 - Reveal secret (S... + mnemonic)
                                 - Import existing (on first-run only)
                                 - optional Horizon balance probe
NEW src/crypto/stellar/          per decision §8.1 — small in-tree
                                 crypto module rather than a Conan dep:
  bip39.{hpp,cpp}                  entropy ↔ mnemonic ↔ seed (PBKDF2)
                                   vendored from trezor-firmware/crypto/
                                   bip39.c (MIT, attribution preserved)
  bip39_english_wordlist.cpp       2048-word list, vendored from
                                   trezor-firmware/crypto/bip39_english.c
                                   (MIT, attribution preserved)
  slip10_ed25519.{hpp,cpp}         SLIP-0010 hardened ed25519 CKDpriv;
                                   the ed25519 path of trezor's
                                   bip32.c hdnode_from_seed_curve +
                                   hdnode_private_ckd, lifted (MIT)
  strkey.{hpp,cpp}                 base32 + CRC16-XMODEM with version
                                   byte; ported from stellar-core
                                   src/crypto/StrKey.cpp (Apache-2.0)
                                   — small enough to write fresh
```

### 3.6 Risks

- **User loses the seed → loses every published share code forever.**
  This is the standard self-custodial trade-off and is the explicit
  cost of the "no portal" design choice. Mitigate with strong
  first-run backup affordance + clear warnings. Document in artist
  onboarding.
- **Migration edge cases.** Files partially populated by older
  `dev_mint_token.py` versions, or with corrupted strkey checksums.
  Load path stays defensive about field presence + format detection.
- **BIP-39 / SEP-0005 implementation correctness.** Wrong derivation
  path = different keypair = total identity loss for early users.
  Mitigated structurally by vendoring trezor-firmware/crypto/ (§8.1) —
  Trezor's BIP-39 + SLIP-0010 ed25519 implementation is battle-tested
  in millions of hardware wallets and matches every major Stellar
  wallet's derivation. Validated additionally with cross-checks
  against Lobstr / Solar during development — same mnemonic in both
  should produce the same `G...` address.

---

## 4. Workstream B — Tagged-file auto-hosting

### 4.1 The shape

Today, hosting requires the user to:
1. Open a canvas in the file-select view.
2. Click "Host" in the Toolbar.
3. Pick SUBSCRIPTION mode.
4. Keep the file open.

After Workstream B:
1. Open a canvas, hit a new **"Publish"** toggle on the file (or in
   the Toolbar's Canvas Settings menu).
2. The file's metadata stores `published: true`.
3. From now on, every time Inkternity launches, that file is
   auto-loaded as a background hosted SUBSCRIPTION session — no
   button press required, no drawing UI loaded for it.
4. The artist can still open one of those canvases for editing in
   the foreground (which... raises a state question — see §4.4).
5. The artist can flip `published: false` to stop auto-hosting.

The wire-format and signaling-server interaction are unchanged: each
published canvas just runs the same `World::start_hosting` code path
the Host button already runs, with `hostMode = SUBSCRIPTION` and the
§12.5-derived share code.

This is **not** a daemon. Closing Inkternity stops the hosting.
Launching Inkternity (even just to file-select view) starts it again.
The artist's mental model is: *"Inkternity launched = my published
canvases are reachable. Inkternity closed = they're not."*

### 4.2 File-format change

Adding to the existing `.inkternity` file metadata:

```json
{
  "schemaVersion": "...",
  "canvasId": "...",
  "artistMemberPubkey": "G...",
  "appPubkeyAtPublish": "G...",
  "published": true,         // NEW — auto-host on app launch
  "publishedAt": "2026-05-14T18:30:00Z"   // NEW — for sort/display only
  ...
}
```

`published: false` (or absent) → existing behavior, no auto-host. The
field is independent of `has_subscription_metadata()` — but UI
gating should only let the artist *set* `published: true` when the
canvas has portal-issued subscription metadata, same rule as the
Host menu's SUBSCRIPTION button.

### 4.3 App-launch sequence

> **Implementation reality found mid-build:** the cap-of-3 simultaneous
> hosts originally scoped in §8.5 is **not currently buildable** — see the
> NetLibrary architectural constraint below. Phase 1 ships **cap-of-1**;
> the cap-3 promise reverts to a future workstream gated on a NetLibrary
> refactor. The §8.5 decision is amended accordingly.

In `main.cpp` after `MainProgram` construction, before the file-select
screen renders its first frame:

```cpp
PublishRegistry r;
r.load(configPath);
if (auto p = r.published_path(); p.has_value() && file_exists(*p)) {
    // Spawn the single background-hosted World for this file.
    main.backgroundHost = make_background_world(*p);
}
```

**NetLibrary single-host constraint.** The current `NetLibrary` is
process-singleton on `globalID` and the signaling websocket — the WSS
URL is built once in `init()` as `signalingAddr + "/" + globalID`, with
one `ws` connection. Two `World` instances trying to host SUBSCRIPTION
mode in parallel would each need their own websocket on their own
WSS-path globalID, which the current code can't open. This is not a
memory or perf concern — it's a structural one.

Until a future workstream rebuilds NetLibrary's signaling lifecycle to
be per-NetServer (separate `ws` + `globalID` per active host), Phase 1
auto-hosts **at most one canvas in the background at a time**. The
artist marks a canvas "published"; on app launch, that single canvas
gets a background-hosted `World`. Toggling Publish on a different
canvas un-publishes the previous one (with confirmation: *"This will
replace the currently published canvas <name>. Continue?"*).

For the single background `World`: spawn a full `World` instance
(rendering scaffolding included — slim-down is premature per the
original §4.3 reasoning). The instance lives alongside `main.world`
in a new `main.backgroundHost` slot; the main loop only renders
`main.world`. The background instance ticks via the existing
NetLibrary update path which already iterates registered NetServers.

**Power-user escape hatch — multi-instance.** Inkternity has no
single-instance lock. An artist who genuinely needs N>1 canvases
hosted simultaneously today can launch N copies of the desktop app;
each process gets its own NetLibrary singleton, its own ws connection,
and can independently host one published canvas. Caveat: all
instances share `configPath` (correct — same `inkternity_dev_keys.json`,
same Stellar identity), and would race on the single
`inkternity_published.json` registry — so this works in practice only
once the registry is overridable per-process. A future small
iteration adds an `--auto-host=<path>` CLI flag that bypasses the
registry on that process. Documented here so the architectural option
isn't lost; not built into Phase 1.

### 4.4 What happens when the artist opens the published canvas?

Two-step lifecycle, made simple by the cap-1 constraint:

1. **Open the published canvas → tear down background, foreground hosts.**
   The background `World` instance is destroyed (its NetServer shuts down,
   its WSS connection drops). The newly-loaded foreground `World` then
   starts hosting normally as if the artist had clicked Host. Subscribers
   experience a brief drop + reconnect.
2. **Close the foreground canvas → restart background.** When the artist
   leaves the canvas (back to file-select), if it's still in the publish
   registry, spawn a fresh background `World` for it.

**Open a *different* (unpublished) canvas → background keeps running.**
The two `World`s coexist; one ticks visibly (foreground), the other
ticks invisibly (background) servicing subscribers. Memory cost is 2 ×
per-canvas footprint while both are active.

This avoids the §4.4 "promote to foreground" state-shuffling entirely —
the foreground always loads from disk, the background always loads
from disk, they never share state. Subscribers see a brief
disconnection on open and another on close; for a publishing flow
where the artist mostly *isn't* editing the published canvas, that's
fine.

### 4.5 UI surfaces

```
src/Screens/FileSelectScreen.cpp Files tab: a "● Published" badge on
                                 the single published file. Settings
                                 panel: a small "Auto-hosting" block
                                 showing what's published (or "Nothing
                                 published") + connection count. The
                                 badge doubles as the click target to
                                 unpublish.
src/Toolbar.cpp                  Canvas Settings menu: "Publish to
                                 subscribers" toggle. Disabled when
                                 has_subscription_metadata()==false
                                 with the same explanatory note as
                                 the existing Host menu SUBSCRIPTION
                                 button. Enabling on canvas Y when
                                 canvas X is already published prompts:
                                 "This will replace the currently
                                 published canvas <name>. Continue?"
src/World.cpp                    Reused as-is — the background instance
                                 is a full `World`, just never gets
                                 main-loop draw/update calls. No new
                                 ctor or "headless" mode (deferred per
                                 the original §4.3 "premature" note).
src/main.cpp                     post-init pass: read PublishRegistry,
                                 if present spawn the single background
                                 World; on file-open / file-close,
                                 stop/restart the background slot.
NEW src/PublishRegistry.{hpp,cpp}
                                 Single-file JSON helper:
                                 inkternity_published.json holds the
                                 single { path, publishedAt } entry.
                                 Cap-1 by structure (single field,
                                 not a collection). Future cap-N
                                 revisits this shape.
```

### 4.6 Surfaces NOT touched

- No new binary.
- No CMake target split.
- No platform service / launchd / systemd integration.
- No portal endpoint.
- No signaling-server change.
- No notarization complexity (one app, one DMG, like today).

### 4.7 Risks

- **Memory cost of N simultaneously-loaded canvases.** Mitigation:
  measure with 5 / 10 / 20 published canvases on a typical machine;
  cap the auto-host count with a setting if it bites.
- **State-shuffling bugs when promoting a background canvas to
  foreground** (§4.4). The current code path assumes a clean
  load-from-disk into a fresh `World`; reusing an already-live
  `World` as the editing surface is new territory.
- **Artist confusion: "why is my CPU/network busy when I'm not
  hosting?"** Mitigation: clear status panel in file-select Settings
  showing what's published + what's connected.

---

## 5. How A and B interact

Lightly. The only real coupling: tagged-file auto-hosting derives its
share code from `(app_secret, canvas_id)` via §12.5, so the
Stellar-encoding migration in Workstream A has to land *before or
with* Workstream B — otherwise the encoding change shifts the
on-disk format of `app_secret` mid-flight and the auto-host pass at
app launch has to deal with both formats. Ship order: **A.1 (strkey
migration) → A.2 (UI surface + import) → B (auto-hosting)**.

---

## 6. Out of scope / explicitly tabled

- **True headless / background-service daemon.** Inkternity running
  without a window (system-tray icon on Windows, menu-bar item on
  macOS, systemd user service on Linux), serving canvases 24/7
  whether or not the desktop app is launched. Not in Phase 1 — the
  process-model change, packaging change, and notarization change
  combined are a significant project, and the tagged-file approach
  covers most practical scenarios. Revisit when there's a concrete
  artist asking for 24/7 hosting they can't get from "leave my
  machine on with Inkternity launched."
- **Portal-mediated backup / restore.** Could be added later as an
  opt-in if artists ask for it, but Phase 1 stays self-custodial.
- **Automatic on-ledger settlement of subscription payments.** Phase 0
  tokens are portal-signed JWTs; making them direct Stellar payment
  claims is bigger surgery.
- **Cloud-hosted canvas serving** (HEAVYMETA-as-a-service for artists
  who don't want to run their own machine). Possible future revenue
  line; out of scope.
- **TURNS (TURN-over-TLS).** Phase 0 documents this as deferred;
  Phase 1 doesn't change the calculus.

---

## 7. Open product questions

_(All settled — see §8 for the resolved decisions and their rationale.
Questions retained here in their original form for the design-doc
audit trail.)_

1. **BIP-39 / SEP-0005 implementation:** roll our own or pull a library?
2. **Mnemonic length:** 12 words or 24?
3. **Wordlist:** English-only or multi-language?
4. **Foreground-edit behavior on opening an auto-published canvas:**
   disable editing or promote to foreground?
5. **Auto-host cap:** none / soft cap with warning / hard cap?
6. **Horizon polling cadence for the funded-balance indicator:**
   on-demand or background poll?

---

## 8. Decisions log

1. **BIP-39 / SEP-0005: roll our own, vendoring Trezor MIT sources.**
   Library survey (overcat/libstellar, StellarQtSDK, libwally-core,
   stellar-core, ciband/bip39, trezor-firmware/crypto): no permissive
   C++ package on ConanCenter covers all three primitives we need
   (BIP-39 + SLIP-0010 ed25519 derivation + Stellar strkey). The
   Stellar Development Foundation maintains no official C++ SDK; the
   one Qt-based community SDK is too heavy. **Decision:** vendor the
   relevant files from `trezor-firmware/crypto/` (MIT) — `bip39.c`,
   `bip39_english.c` (the 2048-word list), `bip32.c`'s
   `hdnode_from_seed_curve` + `hdnode_private_ckd` for SLIP-0010
   ed25519, with `sha2`/`hmac`/`pbkdf2` either vendored or stubbed to
   call our in-tree SHA-512 / HMAC-SHA-512. Total ~600 LOC of new
   code on top of the existing crypto in-tree. Strkey (~100 LOC)
   written from scratch using `stellar-core/src/crypto/StrKey.cpp`
   (Apache-2.0) as a reference. Trezor's BIP-39 / SLIP-0010 are
   battle-tested in millions of hardware wallets, which mitigates the
   derivation-correctness risk in §3.6 better than writing from spec
   would.

2. **Mnemonic length: 12 words.** 128-bit entropy. Easier to back up
   than 24, matches the default of most Stellar wallets (Lobstr,
   Solar, Freighter), and the entropy is well above any realistic
   brute-force threat model for self-custodial keys protecting a
   share-code namespace.

3. **Wordlist: English-only.** Cross-wallet portability matters more
   than localization at this scale. Artists who want a non-English
   mnemonic can import their English seed into another Stellar wallet
   and let that wallet display it in their preferred language.

4. **Foreground-edit when opening an auto-published canvas: promote
   to foreground.** Matches the artist's mental model — "I published
   this, now I'm editing it, subscribers see my updates." The §12.2
   viewer-only wire enforcement already prevents subscribers from
   writing back, so promoting the live `World` to the editing
   surface is safe.

5. **Auto-host cap: hard cap at 3 canvases — AMENDED to cap-of-1.**
   Original cap-3 was set on the assumption that "multiple full World
   instances" was the only constraint (per §4.3). When implementation
   started, NetLibrary turned out to be process-singleton on `globalID`
   and the signaling websocket — multiple simultaneous SUBSCRIPTION
   hosts would each need their own ws-on-different-WSS-path, which the
   current code doesn't support. Phase 1 ships **cap-of-1**. UI surfaces
   exactly one "Publish" toggle that's mutually exclusive across all
   canvases (toggling on one auto-unpublishes the previously-published
   one, with an explicit confirmation). A future workstream that
   refactors NetLibrary to per-NetServer signaling lifecycle will
   restore the cap-3 (or higher) intent.

6. **Horizon polling: on Settings-open only.** No background poll,
   no periodic refresh. Minimal network chatter for the vast
   majority of users who never look at the funded-balance indicator;
   when they do open Settings they see the current balance from a
   single HTTP GET to `https://horizon.stellar.org/accounts/<G...>`.
