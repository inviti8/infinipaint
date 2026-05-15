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

> **Architectural reality, baked in from the start of this section.** The
> initial scoping (cap-of-3 in §8.5, single shared
> `inkternity_published.json` under `configPath`) ran into two real
> constraints during build:
>
> 1. `NetLibrary` is process-singleton on `globalID` + the signaling
>    websocket. A single Inkternity process can host **at most one**
>    canvas in SUBSCRIPTION mode at a time.
> 2. `configPath` is shared across every Inkternity process on the
>    machine — a central single-slot registry under it can't represent
>    "instance A hosts canvas X, instance B hosts canvas Y."
>
> The §4 design below is the *redesigned* model that takes both as
> givens. Per-canvas marker + per-canvas runtime lock + multi-instance
> as the multi-canvas story. The original cap-of-3 / central registry
> framing is gone; §8.5 is amended.

### 4.1 The shape

Today, hosting requires the user to:
1. Open a canvas in the file-select view.
2. Click "Host" in the Toolbar.
3. Pick SUBSCRIPTION mode.
4. Keep the file open.

After Workstream B:
1. Open a canvas, hit a new **"Publish"** toggle in the Toolbar's
   Canvas Settings menu.
2. A `<name>.inkternity.publish` sidecar JSON is written next to the
   canvas file.
3. From now on, when Inkternity launches with that canvas in its
   saves directory, it claims a per-canvas runtime lock and serves
   the canvas in the background — no button press required.
4. The artist can flip the toggle off (deletes the sidecar) to stop
   publishing.

**Per-instance vs per-canvas.** Cap-of-1 hosted canvas per process is
*structural* (NetLibrary). The artist who needs N hosted canvases
launches N Inkternity instances; each instance grabs the first
published canvas it can lock. No central cap. No CLI flags, no
single-instance lock — just `Open another Inkternity window` and a
second canvas comes online.

The wire format and signaling server are unchanged. Each running
hosted canvas drives the same `World::start_hosting` code path the
Host button already drives, with `hostMode = SUBSCRIPTION` and the
§12.5-derived share code.

This is **not** a daemon. Closing the Inkternity instance hosting
canvas X stops X. Launching another Inkternity instance picks up
where the previous one left off (it claims X via the lock-file
mechanism described below).

### 4.2 Per-canvas state files

Two sidecar files live next to each canvas (alongside the existing
`.jpg` thumbnail):

```
<name>.inkternity         the canvas itself (binary, opaque)
<name>.inkternity.jpg     thumbnail (existing)
<name>.inkternity.publish JSON marker — present iff "published"
                          { "publishedAt": "2026-05-14T18:30:00Z" }
<name>.inkternity.lock    PID file — present while a live Inkternity
                          instance owns the runtime claim on this
                          canvas. Format: { "pid": <int> }
```

**Why sidecars (not in-file metadata).**
The `.inkternity` binary is ZSTD-compressed cereal — there's no cheap
"read just the metadata header" path; reading `published` would mean
deserializing the whole file at every directory scan. Sidecars are:
- Cheap to scan (filesystem `exists()` check per file).
- Trivially readable + writable in the file-select view, where the
  app doesn't have the canvas loaded.
- Travel with the file across `cp`/`mv`/cloud-sync if the user
  preserves siblings (and we can recover gracefully if they don't —
  the marker is just intent; absence means "not published").

**Marker semantics.** Existence is the only authoritative bit; the
JSON contents are advisory (currently just `publishedAt` for
sort/display). Future fields go here if needed (per-canvas access
controls, expiry, etc.).

**Lock semantics.**
- Acquisition is atomic via `O_CREAT | O_EXCL` (POSIX) /
  `CREATE_NEW` (Windows). Two instances calling
  `try_acquire_lock(canvas)` simultaneously — exactly one wins.
- Lock content is the holder's PID. On scan, a lock whose PID is no
  longer alive (process crashed, killed, or closed without RAII
  release) is treated as **stale** and silently reclaimed by the next
  instance that wants it.
- Released via RAII on graceful shutdown
  (`PublishedCanvases::release_all_held()` from MainProgram dtor),
  or by stale-detection on subsequent launches.

The marker is independent of `has_subscription_metadata()` — but the
Toolbar gates *setting* the marker on
`has_subscription_metadata() == true`, same rule as the Host menu's
SUBSCRIPTION button. A canvas with the marker but no metadata is
inert (the launch-time scanner ignores it once the runtime auto-host
work lands).

### 4.3 App-launch sequence

In `main.cpp` after `MainProgram` construction, before the file-select
screen renders its first frame:

```cpp
mS.m->hostedCanvasPath = PublishedCanvases::claim_first_available(
    mS.m->conf.configPath / "saves");
if (mS.m->hostedCanvasPath) {
    main.backgroundHost = make_background_world(*mS.m->hostedCanvasPath);
}
```

`claim_first_available` walks `saves/`, gathers every canvas with a
marker, and tries to acquire each lock in turn. Returns the first
canvas it successfully locked, or `nullopt` (no published canvases,
or all are locked by other live Inkternity instances). The returned
path is the canvas this process will background-host.

**For the single background `World`** (deferred — see §4.6 known
gap): spawn a full `World` instance. The instance lives alongside
`main.world` in a `main.backgroundHost` slot; the main loop only
renders `main.world`. The background instance ticks via the existing
NetLibrary update path which already iterates registered NetServers.
No new "headless" mode — slim-down is premature per the original §4.3
reasoning.

**Multi-instance flow:**
1. Artist launches Inkternity. It scans `saves/`, locks canvas A
   (which has a marker), spawns the background-hosted `World` for A.
2. Artist launches a second Inkternity. It scans `saves/`, sees A
   is locked (live PID), tries B (also marked), locks it, spawns the
   background-hosted `World` for B.
3. Artist launches a third. Sees A and B locked, tries C, etc.
4. Closing instance #2 releases B's lock. A subsequent fourth launch
   picks B back up.

No CLI args. No per-instance config dirs. The OS process boundary
*is* the per-instance scope; the per-canvas lock file is the
coordination primitive between instances.

### 4.4 What happens when the artist opens the published canvas?

Cap-1-per-process makes this clean:

1. **Artist opens the canvas this instance is currently background-
   hosting.** The lock is released, the background `World` is torn
   down, the foreground `World` loads from disk and re-acquires the
   lock + starts hosting normally. Subscribers see a brief drop +
   reconnect.
2. **Artist closes back to file-select.** If the canvas is still
   marked published, foreground releases the lock, the background
   `World` is re-spawned, takes the lock, resumes hosting.

**Artist opens a *different* canvas (any state).** Background keeps
running. Two `World`s coexist; foreground for editing, background for
serving. Memory cost is 2 × per-canvas footprint while both are
active.

**Artist opens a published canvas hosted by a *different* Inkternity
instance.** The other instance owns the lock. This instance can still
load the file for editing — but trying to Host (manual or auto)
fails because the lock can't be acquired. UI shows
"Hosting (another instance)" on the file in the file list; the
Toolbar's Host menu disables SUBSCRIPTION mode with a note pointing
the artist to the other window.

This avoids the original §4.4 "promote to foreground" state-shuffling
entirely — foreground and background always load from disk, never
share state. Subscribers see a brief disconnection on open and
another on close; for a publishing flow where the artist mostly
*isn't* editing the published canvas, that's fine.

### 4.5 UI surfaces

```
src/Screens/FileSelectScreen.cpp Files tab: per-file caption row
                                 reflecting the lock state:
                                   "* Hosting (this instance)"
                                   "* Hosting (another instance)"
                                   "* Published (idle)"
                                 Settings tab: a small "Auto-hosting"
                                 block showing what THIS instance has
                                 locked at startup + a count of
                                 markers found in saves/. When
                                 nothing is locked but markers exist,
                                 explains the lock-by-other-instance
                                 case.
src/Toolbar.cpp                  Canvas Settings menu: "Publish to
                                 subscribers" toggle (writes the
                                 marker sidecar). Disabled when
                                 has_subscription_metadata()==false
                                 with the same explanatory note as
                                 the existing Host menu SUBSCRIPTION
                                 button. No "replace" confirmation —
                                 multiple canvases can be published
                                 independently.
src/World.cpp                    Reused as-is — the background
                                 instance is a full `World`, just
                                 never gets main-loop draw/update
                                 calls. No new ctor or "headless"
                                 mode.
src/main.cpp                     post-init pass: scan saves/, claim
                                 first available, record on
                                 `hostedCanvasPath`. (Spawning the
                                 background World is the deferred
                                 piece.)
NEW src/PublishedCanvases.{hpp,cpp}
                                 Namespace with marker + lock helpers:
                                 is_published / set_published /
                                 clear_published, try_acquire_lock /
                                 release_lock / is_locked_by_us /
                                 is_locked_by_anyone,
                                 scan_published / claim_first_available /
                                 release_all_held.
                                 Stale-PID detection cross-platform
                                 (OpenProcess on Windows, kill(pid,0)
                                 on POSIX).
```

### 4.6 Surfaces NOT touched

- No new binary.
- No CMake target split.
- No platform service / launchd / systemd integration.
- No portal endpoint.
- No signaling-server change.
- No notarization complexity (one app, one DMG, like today).
- **No central registry** — the previously-scoped
  `inkternity_published.json` under `configPath` is gone, removed
  during the redesign. State that needs to be per-canvas lives with
  the canvas (sidecars); state that needs to be per-instance lives
  in process memory + the lock file's PID.

**Known gap (intentional, not a regression):** the marker + lock
machinery and the launch-time `claim_first_available` are wired up,
but spawning the actual background-hosted `World` for the locked
canvas is not yet built. Today the lock is acquired and recorded on
`MainProgram::hostedCanvasPath`; the artist still has to open the
canvas and click Host manually. That last step (constructing a
non-foreground `World` from disk + driving it through
`init_net_library` + `start_hosting`) is the next iteration of B.

### 4.7 Risks

- **Stale lock files surviving a hard crash.** Mitigated by the
  PID-alive check on every acquisition attempt — if a lock exists
  but its PID is gone, the lock is reclaimed silently. Worst case is
  a brief delay on the next launch while the OS-process check
  resolves.
- **PID reuse on long-running systems.** A reclaimed PID could in
  principle be a different process unrelated to Inkternity. The
  acquire-then-write atomicity protects against the wrong-process
  case for *new* lock acquisitions; for *stale-detection*, a
  reused PID just means we leave the lock file alone (treat as
  alive). Result: a marker stays "Hosting (another instance)" until
  Inkternity actually launches and reclaims it. Tolerable.
- **Marker drift on file moves.** If the user moves
  `mycanvas.inkternity` without also moving `mycanvas.inkternity.publish`,
  publish state is lost. Same risk class as the existing `.jpg`
  thumbnail. Document; consider a future `World::save_to_file` hook
  that renames sidecars.
- **Memory cost of 2 × `World` (foreground + background) when the
  artist has the published canvas open in foreground.** Acceptable;
  this is an artist's primary publishing flow, not the common case.
- **Artist confusion: "why is my CPU/network busy when I'm not
  hosting anything?"** Mitigation: clear file-list captions + the
  Settings auto-hosting block.

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

5. **Auto-host cap: hard cap at 3 canvases — REPLACED with per-canvas
   marker + per-instance lock + multi-instance for multi-canvas.**
   Two iterations on this:

   - First amendment: cap-of-1 in-process (hit when implementation
     surfaced the NetLibrary process-singleton constraint).
   - Second redesign (current): per-canvas `.publish` marker + per-canvas
     `.lock` PID file. The cap-1 *per process* is structural and stays;
     the cap-N *globally* falls out of launching N Inkternity instances,
     each grabbing a different published canvas from the per-canvas
     lock-file pool (§4.2 / §4.3).

   The original framing of "auto-host cap" doesn't apply any more —
   there's no central limit, just structural cap-1-per-process and
   the artist's choice of how many instances to launch. UI shows
   per-file lock state ("Hosting (this instance)" / "Hosting (another
   instance)" / "Published (idle)") rather than a cap-vs-current
   counter.

   A future NetLibrary refactor (per-NetServer ws + globalID) would
   raise the structural per-process cap above 1 — but the per-canvas
   marker model already absorbs that change without UX rework: one
   instance would just lock multiple canvases instead of one.

6. **Horizon polling: on Settings-open only.** No background poll,
   no periodic refresh. Minimal network chatter for the vast
   majority of users who never look at the funded-balance indicator;
   when they do open Settings they see the current balance from a
   single HTTP GET to `https://horizon.stellar.org/accounts/<G...>`.
