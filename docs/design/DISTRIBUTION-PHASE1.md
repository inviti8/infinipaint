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

> **Architectural reality, baked in from the start of this section.**
> The §4 design below has been through three iterations:
>
> 1. **Initial:** cap-of-3 in §8.5, single shared
>    `inkternity_published.json` under `configPath`.
> 2. **First redesign:** per-canvas marker + per-canvas runtime lock +
>    artist manually launches N Inkternity instances to host N canvases.
>    Forced by two implementation constraints:
>    1. `NetLibrary` is process-singleton on `globalID` + the signaling
>       websocket. A single Inkternity process can host **at most one**
>       canvas in SUBSCRIPTION mode at a time.
>    2. `configPath` is shared across every Inkternity process on the
>       machine — a central single-slot registry under it can't
>       represent "instance A hosts canvas X, instance B hosts canvas Y."
> 3. **Current — multi-process, headless side-instances.** The artist
>    runs *one* Inkternity. On launch, it spawns one headless
>    side-instance per published canvas (no window, no Skia GPU
>    context, ~30–80 MB each). Each side-instance hosts its assigned
>    canvas; the main process stays focused on editing. The cap-1 /
>    config-shared constraints from (2) still hold structurally — the
>    multi-process model is how we get N hosts without forcing the
>    artist to launch multiple windows.
>
> The current model takes (3) as the design contract. The cap-3
> framing is gone (§8.5 amended). The "manually launch N instances"
> framing from iteration 2 is gone too — the main process now
> orchestrates the side-instance fleet automatically.

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
3. The next time the artist launches Inkternity (or immediately, if
   it's already running — see §4.3), the main process **spawns a
   headless side-instance** of itself, passing `--host-only <path>`.
   That side-instance is a child OS process: no SDL window, no Skia
   GPU context, no UI — just `World` + NetLibrary serving the canvas.
   The artist's main window is unaffected.
4. The artist can flip the toggle off (deletes the sidecar) to stop
   publishing. The main process signals the corresponding
   side-instance to exit.

**One artist, one main window, N background hosts.** Cap-of-1 hosted
canvas per process is structural (NetLibrary is all-static). We get
N concurrent hosts by spawning N processes — but the artist never
manages those processes directly. The main process is the
orchestrator; the side-instances are short-lived workers it owns.

**Why headless side-instances rather than in-process background
`World`s.** Two reasons fall out of the spike (§4.8):

1. **Structural fit.** `NetLibrary` is process-singleton (`alreadyInitialized`
   atomic + all-static state, all-static `peers` map / `globalID` /
   `ws`). One process really can host only one canvas. Building two
   `World`s in the same process would fight over the same NetLibrary
   slot.
2. **Memory and resource cost.** A side-instance with no window, no
   `GrDirectContext`, and no `SDL_INIT_VIDEO` is ~30–80 MB instead of
   the ~200–500 MB a full UI process would cost. 10 published canvases
   becomes ~500 MB instead of ~3 GB. Phase 1 cost concern essentially
   evaporates.

The wire format and signaling server are unchanged. Each running
side-instance drives the same `World::start_hosting` code path the
Host button already drives, with `hostMode = SUBSCRIPTION` and the
§12.5-derived share code. Signaling-server handoff is trivially
clean: `inkternity-server/signaling/server.py` explicitly replaces
prior connections from the same `globalID` (`code=1000 reason="superseded"`),
so when the main process takes over a canvas from its side-instance,
subscribers' WebRTC peer connections re-negotiate against the new
holder without manual coordination.

This is **not** a daemon. Closing the artist's Inkternity stops *all*
of its side-instances — they're children of the main process and
exit when the main process does (orphan detection in the
side-instance polls for parent PID and self-terminates if main
dies abnormally; see §4.4).

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
  `CREATE_NEW` (Windows). Two processes calling
  `try_acquire_lock(canvas)` simultaneously — exactly one wins.
- Lock content is the holder's PID. The holder can be either a
  side-instance (the normal case, when the canvas is background-hosted)
  or the main process (the takeover case, when the artist is editing
  the published canvas in foreground). The acquire-release semantics
  are identical for both.
- On scan, a lock whose PID is no longer alive (process crashed,
  killed, or closed without RAII release) is treated as **stale**
  and silently reclaimed by the next process that wants it.
- Released via RAII on graceful shutdown
  (`PublishedCanvases::release_all_held()` from MainProgram dtor in
  the main process, or from the `--host-only` shutdown path in a
  side-instance), or by stale-detection on subsequent launches.

The marker is independent of `has_subscription_metadata()` — but the
Toolbar gates *setting* the marker on
`has_subscription_metadata() == true`, same rule as the Host menu's
SUBSCRIPTION button. A canvas with the marker but no metadata is
inert (the launch-time scanner ignores it once the runtime auto-host
work lands).

### 4.3 App-launch sequence

There are two distinct startup paths in the same binary now:

**Main process (UI):** unchanged through `MainProgram` construction.
After config + DevKeys load, before the file-select screen renders
its first frame:

```cpp
// Scan saves/ for canvases with publish markers. For each one we can
// claim (i.e. nobody else holds the lock), spawn a side-instance.
auto candidates = PublishedCanvases::scan_published(
    mS.m->conf.configPath / "saves");
for (const auto& canvasPath : candidates) {
    if (PublishedCanvases::is_locked_by_anyone(canvasPath)) continue;
    mS.m->sideInstances.spawn(canvasPath);  // SDL_CreateProcess
}
```

`sideInstances.spawn(path)` builds argv =
`{self_exe, "--host-only", path.string(), nullptr}` and calls
`SDL_CreateProcess(argv, /*pipe_stdio=*/true)`. The piped stdio is
how the main process signals graceful shutdown later (write `"STOP\n"`).
It keeps the `SDL_Process*` and the canvas path in a map keyed by
canonical path, used for handoff and shutdown.

**Side-instance (`--host-only`):** an early dispatch in `main.cpp`,
*before* `initialize_sdl` / `MainProgram` / window creation — same
bypass pattern as the existing `--mypaint-*` flags and the
`--test-*` harness flags (§4.8). The headless path:

```cpp
if (auto rc = HostOnly::dispatch(argc, argv)) {
    return *rc == 0 ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
}
```

Inside `HostOnly::dispatch`, when the flag matches:

1. Minimal SDL init (no `SDL_INIT_VIDEO`).
2. Logger to a per-canvas file (e.g. `<configPath>/logs/host-<name>.log`).
3. Construct a `MainProgram` instance whose `window` struct stays
   uninitialised (no `sdlWindow`, no GPU `ctx`, no surfaces). Set
   `window.size` to a placeholder so `World`'s ctor doesn't trip.
4. Load DevKeys.
5. `try_acquire_lock(canvas_path)` — if it fails (another process beat
   us), exit 2.
6. Construct `World` for the canvas via the normal load path.
7. `world.start_hosting(SUBSCRIPTION, ...)`.
8. Idle loop: tick `NetLibrary::update()` and a single check per
   iteration of (a) parent-process alive (PPID poll on POSIX,
   `OpenProcess`+`WaitForSingleObject` on Windows), (b) stdin has
   "STOP\n" pending. Either trigger → graceful exit: save canvas,
   release lock, return 0.

`World::update()` is a one-liner (`connection_update()`), so the
side-instance's main loop is small. `World::draw()` is never called.

**Result, in artist-visible terms:**
1. Artist launches Inkternity. Main process appears. In the background,
   side-instances for each published canvas come online over the next
   ~1–2s and start serving subscribers.
2. The artist edits a *different* canvas in foreground; the
   side-instances keep running undisturbed.
3. Closing Inkternity stops everything (main signals all side-instances,
   waits for their exits, then itself exits).

No CLI args for the artist. No `Open another window` instructions.
The OS-process boundary is the per-host scope; the per-canvas lock
file remains the coordination primitive between the main process,
its side-instances, and (for resilience) any leftover processes from
a prior session.

### 4.4 What happens when the artist opens the published canvas?

Cross-process kill-and-takeover handles this cleanly. Hosting moves
between processes; the canvas keypair and `globalID` derive from
`(app_secret, canvas_id)` and are stable across the transfer, so
subscribers see a brief reconnect rather than a session reset.

**Artist opens a canvas the main process knows is being background-
hosted by one of its side-instances:**
1. Main process looks up the side-instance for that canvas path in
   `sideInstances` map.
2. Main writes `"STOP\n"` to the side-instance's stdin.
3. Side-instance receives STOP → saves canvas → releases lock → exits 0.
4. Main waits on `SDL_WaitProcess` with a timeout (~2s). If the
   side-instance doesn't exit gracefully, `SDL_KillProcess` it and
   reclaim the lock via stale-PID detection.
5. Main acquires the lock for the canvas.
6. Main loads the canvas in foreground (`MainProgram::create_new_tab`)
   and starts hosting via the existing Host menu code path.
7. Subscribers' WebRTC peer connections drop, reconnect through
   signaling, and the server replaces the old globalID slot with the
   main process's WSS connection (`code=1000 reason="superseded"`
   from `signaling/server.py`).

**Artist navigates back from the published canvas to file-select:**
1. Foreground world is destroyed normally (existing tab-close path).
2. As part of that destruction (or in a follow-up step), main
   releases the canvas's lock.
3. If the publish marker is still present, main spawns a fresh
   side-instance for that canvas.
4. Side-instance acquires the lock and resumes hosting. Subscribers
   reconnect again.

**Artist opens a *different* canvas (any state).** Side-instances
keep running untouched. The main process holds zero hosting work and
just edits in foreground. No memory doubling — the foreground
`World` is the only `World` in the main process; the host `World`s
live in their respective side-instances.

**Artist opens a canvas published in a corrupt or unknown state**
(e.g., marker present but no side-instance is alive holding the
lock — possible if the side-instance crashed). Main's
`is_locked_by_anyone()` returns false (stale-PID path), main
acquires the lock for foreground hosting normally. Side-instance
respawns when the artist navigates back.

**Edge: subscriber connected during the handoff window.** Between
step 3 (side-instance exits) and step 6 (main starts hosting), there
is a brief window — typically <500ms — where a new subscriber
trying to connect finds no signaling-server entry for the globalID.
Their client retries; reconnect succeeds once main is up. Acceptable
for Phase 1.

**Edge: main process killed -9 with side-instances running.** The
orphan-detect path in each side-instance polls for parent PID (~200ms
cadence per the test harness, §4.8) and self-terminates on
parent-gone. Worst case is a ~1s window of "main is dead, side
still serving" — harmless. Lock files are released by the
side-instances' own RAII shutdown handlers (graceful exit triggered
by the orphan-detect path).

### 4.5 UI surfaces

```
src/Screens/FileSelectScreen.cpp Files tab: per-file caption row
                                 reflecting the lock state:
                                   "* Hosting (this instance)"
                                   "* Hosting (a side-instance)"
                                   "* Hosting (another Inkternity)"
                                   "* Published (idle)"
                                 Settings tab: an "Auto-hosting" block
                                 listing each canvas this Inkternity
                                 is hosting (via main process or a
                                 side-instance), with a summary count.
src/Toolbar.cpp                  Canvas Settings menu: "Publish to
                                 subscribers" toggle (writes the
                                 marker sidecar). Disabled when
                                 has_subscription_metadata()==false
                                 with the same explanatory note as
                                 the existing Host menu SUBSCRIPTION
                                 button. Side effect of enabling: main
                                 process spawns a side-instance for
                                 this canvas immediately. Side effect
                                 of disabling: main signals the
                                 side-instance to exit, removes marker.
                                 No "replace" confirmation — canvases
                                 are published independently.
src/World.cpp                    Reused as-is for the foreground
                                 hosting path. The headless side-
                                 instance reuses the same World ctor +
                                 World::start_hosting; the
                                 differentiation is that the host-only
                                 path runs in a process with no
                                 SDL window / no Skia GPU context. See
                                 §4.6 for the architectural risk on
                                 the subscriber-wire-op ingestion path
                                 (DrawingProgram), which the runtime
                                 implementation pass will verify.
src/main.cpp                     Two new early-dispatch branches
                                 (before SDL/MainProgram init):
                                   --host-only <path>  side-instance
                                                       entry point
                                   --test-* (§4.8)     harness flags
                                 After MainProgram init: scan saves/,
                                 spawn side-instances for each marked
                                 canvas not already locked.
src/PublishedCanvases.{hpp,cpp}  (already landed) marker + lock
                                 helpers: is_published / set_published
                                 / clear_published, try_acquire_lock /
                                 release_lock / is_locked_by_us /
                                 is_locked_by_anyone, scan_published /
                                 claim_first_available /
                                 release_all_held. Stale-PID detection
                                 cross-platform (OpenProcess on
                                 Windows, kill(pid,0) on POSIX).
NEW src/MainProgram side-instance map
                                 std::unordered_map<canonical path,
                                                    SDL_Process*>
                                 plus spawn/signal/wait helpers. RAII
                                 dtor signals STOP to all + waits.
src/Distribution/ProcessTests.*  (already landed) process spawn/IPC/
                                 lock-handoff/orphan-detect test
                                 harness. See §4.8.
NEW src/Distribution/HostOnly.*  --host-only entry: minimal SDL init,
                                 stub MainProgram window, load
                                 DevKeys, construct World, start
                                 hosting, idle on stdin + parent-PID
                                 poll, graceful exit on STOP.
```

### 4.6 Surfaces NOT touched

- **No new binary.** Same `inkternity.exe` / `inkternity.app`; the
  side-instance is the same executable invoked with `--host-only`.
- No CMake target split.
- No platform service / launchd / systemd integration. Side-instances
  are short-lived OS children of the artist's running Inkternity,
  not OS-managed services.
- No portal endpoint.
- No signaling-server change. The existing
  `inkternity-server/signaling/server.py` "new connection wins"
  behavior is exactly what we need; verified during the spike.
- No notarization complexity (one app, one DMG, like today).
- **No central registry** — the previously-scoped
  `inkternity_published.json` under `configPath` is gone. State that
  needs to be per-canvas lives with the canvas (sidecars); state that
  needs to be per-instance lives in process memory + the lock file's
  PID.

**Known architectural risk to resolve during implementation:**
the brush-stroke ingestion path on the host side. When a subscriber
in SUBSCRIPTION mode paints, the wire op routes through
`NetServer` → `DrawingProgram` on the host. If that path lazily
uploads tiles to a `GrDirectContext` for display caching, a headless
side-instance (with no GPU context) will crash on the first
incoming stroke. The libmypaint backing surfaces are CPU
(`MyPaintFixedTiledSurface`), so the underlying tile updates are
fine; what needs verification is that no intermediate cache or
display-pre-rasterization step assumes a GPU context exists. We'll
resolve this during the runtime implementation pass and either
confirm it's fine or carve a narrow guard. The test harness (§4.8)
proves the process plumbing in isolation, so this is the only
canvas-side unknown left.

**Known gaps (intentional, in order of remaining work):**
1. `HostOnly::dispatch` / `--host-only` entry point.
2. `MainProgram::sideInstances` map + spawn/signal/wait helpers.
3. Launch-time scan-and-spawn pass.
4. Toolbar Publish-toggle side effects (spawn/kill).
5. Foreground-open / foreground-close hooks for the handoff in §4.4.

### 4.7 Risks

- **Stale lock files surviving a hard crash.** Mitigated by the
  PID-alive check on every acquisition attempt — if a lock exists
  but its PID is gone, the lock is reclaimed silently. Worst case is
  a brief delay on the next launch while the OS-process check
  resolves.
- **PID reuse on long-running systems.** A reclaimed PID could in
  principle be a different process unrelated to Inkternity. The
  acquire-then-write atomicity protects against the wrong-process
  case for *new* lock acquisitions; for *stale-detection*, a reused
  PID just means we leave the lock file alone (treat as alive).
  Result: a marker stays "Hosting (a side-instance)" until
  Inkternity actually launches and reclaims it. Tolerable.
- **Marker drift on file moves.** If the user moves
  `mycanvas.inkternity` without also moving `mycanvas.inkternity.publish`,
  publish state is lost. Same risk class as the existing `.jpg`
  thumbnail. Document; consider a future `World::save_to_file` hook
  that renames sidecars.
- **Side-instance memory cost.** ~30–80 MB per side-instance with
  headless build (no SDL_INIT_VIDEO, no GrDirectContext, no font
  caching, no Clay UI state). Linear in number of published canvases.
  For an artist with 10 published canvases the total background
  footprint is ~500 MB, well within Phase 1 cost tolerance.
- **Subscriber reconnect storm on takeover.** When the artist opens a
  published canvas for editing, all live subscribers' WebRTC peer
  connections drop and re-negotiate against the new holder via the
  signaling server's "supersede" path. For canvases with many
  subscribers this is a brief burst of signaling traffic. Acceptable;
  not a Phase 1 scaling concern.
- **Process orphans.** Main process killed -9 without warning leaves
  side-instances running. Mitigated by parent-PID polling in each
  side-instance (~200ms cadence per the test harness); orphan
  side-instances self-terminate within ~1s of main's death. Lock
  files are released via their RAII shutdown paths.
- **Cross-platform process management quirks.** SDL3
  `SDL_CreateProcess` is portable; the test harness (§4.8) verifies
  the full spawn / kill / stdin-stop / orphan-detect / lock-handoff
  cycle on Windows. macOS and Linux verification deferred to the
  release-readiness pass; no platform-specific code in the side-
  instance entry point, so risk is low.
- **Artist confusion: "why is my CPU/network busy when I'm not
  hosting anything?"** Mitigation: clear file-list captions ("a
  side-instance is hosting this") and the Settings auto-hosting
  block enumerating each background host.
- **Handoff window during foreground takeover.** ~500ms gap between
  side-instance exit and main process binding the globalID. New
  subscriber connection attempts in that window get no response and
  retry; Inkternity's existing client-side reconnect handles it.
  Established subscribers see a WebRTC disconnect → reconnect with
  one round-trip through signaling.

### 4.8 Verification — test harness

The multi-process side-instance design rests on several primitives we
needed to confirm work reliably across platforms before betting the
canvas-handoff flow on them. `src/Distribution/ProcessTests.{hpp,cpp}`
is a self-contained harness exercising each primitive in isolation —
no NetLibrary, no World, no canvas, no UI. Six `--test-*` argv flags
dispatch *before* SDL/MainProgram init (same bypass pattern as
`--mypaint-hello-dab`); each test reports PASS/FAIL on stderr and
exits 0/1.

| Flag | What it verifies |
|---|---|
| `--test-spawn-roundtrip` | `SDL_CreateProcess` + child exits cleanly with code 0 |
| `--test-spawn-kill` | Long-running child + `SDL_KillProcess` + verify process is gone |
| `--test-spawn-stdin-stop` | Piped stdio: parent writes `STOP\n` over stdin, child reads it on its `std::cin`, responds with `STOPPED\n` on stdout, exits 0 |
| `--test-spawn-multi <N>` | N concurrent looping children, all spawned, all alive, all killed cleanly |
| `--test-lock-handoff <path>` | Child claims `PublishedCanvases` lock, parent verifies via `is_locked_by_anyone()`, parent sends STOP, child releases lock + exits, parent verifies lock is gone |
| `--test-spawn-orphan-detect <result>` (phase 1) + `--verify-orphan-detect <pid> <result>` (phase 2) | Side-instance detects parent process death via PPID polling and self-terminates with a result-file trace |

All six pass on Windows as of the harness landing commit. Each test
is small (≤150 LOC), exit-code-driven, and runnable from a script for
CI integration once we wire that up.

**What the harness leaves to the runtime implementation:**

- `World` construction in a headless process (no
  `SDL_CreateWindow`, no `GrDirectContext`). The spike showed
  `World::update()` is a one-liner and `World::draw()` is the only
  Skia-touching method; provisional plan is to stub the `MainProgram::Window`
  fields and never invoke `draw`. Verified during implementation.
- `NetLibrary` lifecycle inside a side-instance (init + start_hosting
  + idle update loop + clean destroy on STOP). All-static; init/destroy
  exist; signaling-server "supersede" semantics already confirmed
  client-side from reading `inkternity-server/signaling/server.py`.
- The brush-stroke ingestion path on the host (§4.6 architectural
  risk). Verified by spinning up a side-instance, connecting a real
  client, painting a stroke, and confirming the host doesn't crash.

The split (harness for plumbing, runtime work for canvas-side) keeps
each implementation phase honest about what it's proving.

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

- **True background-service daemon.** Inkternity running 24/7 as a
  system service (system-tray icon on Windows, menu-bar item on
  macOS, systemd user service on Linux), serving canvases whether
  or not the artist has launched the desktop app. Not in Phase 1 —
  the OS-service registration, packaging change, and notarization
  change combined are a significant project. Note this is **distinct**
  from the in-scope `--host-only` side-instances of §4.3: those are
  short-lived OS children of the artist's running Inkternity, not
  background OS services. Revisit a true daemon when there's a
  concrete artist asking for 24/7 hosting they can't get from
  "leave my machine on with Inkternity launched."
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
   marker + per-canvas lock + main-process-spawned headless
   side-instances.** Three iterations on this:

   - First amendment: cap-of-1 in-process (hit when implementation
     surfaced the NetLibrary process-singleton constraint).
   - Second redesign: per-canvas `.publish` marker + per-canvas
     `.lock` PID file; artist launches N Inkternity instances
     manually to host N canvases.
   - Third (current): same per-canvas marker + lock model, but the
     N instances are headless side-instance OS processes spawned
     *automatically* by the artist's main Inkternity at launch — not
     manually opened windows. Headless because `World::update()` is
     a one-liner and `World::draw()` is the only Skia-touching
     method; a process with no SDL window and no `GrDirectContext`
     runs the host code path fine at ~30–80 MB instead of the
     ~200–500 MB a full UI process would cost. See §4.1 / §4.3 /
     §4.8 for the design and the verification.

   The original framing of "auto-host cap" doesn't apply any more —
   there's no central limit, just structural cap-1-per-process and
   the main process spawning a side-instance per marked canvas. The
   artist doesn't see processes at all; they see a "Publish" toggle
   per canvas and Settings shows what's being hosted. UI shows
   per-file lock state ("Hosting (this instance)" / "Hosting (a
   side-instance)" / "Hosting (another Inkternity)" / "Published
   (idle)") rather than a cap-vs-current counter.

   A future NetLibrary refactor (per-NetServer ws + globalID) would
   raise the structural per-process cap above 1 — at which point we
   could consolidate side-instances into the main process. But the
   current per-canvas marker + side-instance model already absorbs
   that change without UX rework: the main process would just stop
   spawning side-instances and own the hosts directly.

6. **Horizon polling: on Settings-open only.** No background poll,
   no periodic refresh. Minimal network chatter for the vast
   majority of users who never look at the funded-balance indicator;
   when they do open Settings they see the current balance from a
   single HTTP GET to `https://horizon.stellar.org/accounts/<G...>`.
