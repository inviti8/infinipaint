# Inkternity — GitHub Build Pipeline

> **Audience:** the agent (and any human contributor) standing up Inkternity's first GitHub Actions release pipeline.
>
> **Goal of this doc:** define the build, packaging, signing, and release-publishing flow for Inkternity, mirroring the established HEAVYMETA "house style" from `metavinci` and `glasswing` so a fork's release process feels consistent across the ecosystem.

## 1. Product summary

Inkternity has zero CI today. Every release is a manual per-platform compile + package using the recipes in `docs/BUILDING.md`. This doc proposes a single GitHub Actions pipeline that:

- Triggers on semver tag pushes (`v*`) and on manual dispatch.
- Builds three parallel platform jobs (Linux x86_64, Windows x64, macOS arm64).
- Produces installer artifacts per platform (Flatpak, NSIS, DragNDrop).
- Signs Windows + macOS artifacts and notarizes the macOS bundle.
- Publishes a draft GitHub Release with all artifacts attached.

The pipeline shape, secret names, tag conventions, and publish step all match what `metavinci` and `glasswing` already use, so a HEAVYMETA contributor who already understands one repo's release flow understands Inkternity's.

## 2. Inheritance — the HEAVYMETA house style

Distilled from `metavinci/.github/workflows/build-nuitka.yml` and `glasswing/.github/workflows/build-installers.yml`. Anything we adopt below comes from one of those files.

### 2.1 Triggers

```yaml
on:
  push:
    tags: [ 'v*' ]
    paths-ignore: ['**.md', 'docs/**']
  workflow_dispatch:
    inputs:
      skip-notarize:
        description: 'Skip macOS notarization (faster turnaround for testing)'
        type: boolean
        default: false
```

- Semver tag push (`v0.11.0`, `v0.12.0-rc1`, etc.) is the canonical release trigger.
- `workflow_dispatch` for manual one-off builds.
- `-no-notarize` substring in the tag name is also honored as a notarization skip (matches `metavinci` convention) — e.g. tag `v0.11.0-no-notarize` builds + signs but skips Apple's notary submission.
- No PR builds, no nightly schedule. (Both reference repos chose the same.)
- `paths-ignore` skips builds for documentation-only changes.

### 2.2 Job layout

Three parallel build jobs, each owning their platform's full toolchain + packaging, feeding a final aggregator:

```
build-linux   ─┐
build-windows ─┼──> create-release  (gated on tag push, downloads all artifacts)
build-macos   ─┘
```

The `create-release` job uses `softprops/action-gh-release@v1` with `draft: true` and a hand-written body. The default `GITHUB_TOKEN` is sufficient — no PAT needed.

### 2.3 Versioning

- **Tag is the source of truth.** `BUILD_VERSION: ${{ github.ref_name }}` env-injected into every build script.
- A small regex extracts the numeric portion for installer metadata: `(\d+\.\d+(\.\d+)?)`.
- The C++ source's `VersionConstants.hpp` `CURRENT_VERSION_STRING` does NOT need to match the tag at build time — it's the file-format version, not the release version. Convention: bump it independently when the file format changes (matches the existing pattern from PHASE2 / TRANSITIONS / DISTRIBUTION-PHASE0). Document the relationship in BUILDING.md.
- Helper script: `scripts/create_release.py` (mirroring `metavinci`'s) — takes `git tag --sort=-version:refname`, suggests `v<MAJOR>.<MINOR>.<PATCH+1>`. Optional but recommended.

### 2.4 Signing secrets (already provisioned for HEAVYMETA)

These secret names exist in the org / are reused across `metavinci` and `glasswing`. Inkternity reuses the SAME names so HEAVYMETA's existing certs cover the new repo without provisioning new keys:

| Secret | Purpose | Used by |
|---|---|---|
| `WINDOWS_SIGNING_CERT` | base64-encoded PFX | Windows job |
| `WINDOWS_SIGNING_PASSWORD` | PFX password | Windows job |
| `MACOS_APPLICATION_P12` | base64 macOS Developer ID Application cert | macOS job |
| `MACOS_CERT_PW` | macOS cert password | macOS job |
| `APPLE_ID` | Apple developer email | macOS notarization |
| `APPLE_TEAM_ID` | Apple developer team ID | macOS notarization |
| `APPLE_ID_APP_PW` | App-specific password | macOS notarization |

The macOS job runs with `environment: installers` (matches `metavinci`) so secret access is gated on the environment selector — a small but consistent operational hygiene win.

## 3. Inkternity's existing build infrastructure

What we already have to lean on:

- **Conan + CMake.** Dependencies declared in `conanfile.py`; per-platform profiles in `conan/profiles/{linux-x86_64, macOS-arm64, win-x86_64, emscripten}`.
- **Per-platform install assets** committed:
  - `linuxinstall/` — Flatpak metadata (`.yml`, `.metainfo.xml`, `.desktop`).
  - `macosinstall/` — `Info.plist` + `appicon.icns`.
  - `windowsinstall/` — `app.rc`, `icon.ico`, `inkternity.manifest`.
  - `android-project/` — Gradle project (Phase 2 target — see §10).
  - `emscripteninstall/` — HTML shells for the web build (Phase 2 target).
- **CPack already wired.** `BUILDING.md` shows `cpack -G NSIS` (Windows) and `cpack -G DragNDrop` (macOS) work today. No need to introduce WiX or hdiutil scripting from scratch.
- **`docs/BUILDING.md`** has reproducible per-platform recipes the workflow scripts can adapt.

What we don't have:

- No `.github/workflows/`. `.github/` only contains `FUNDING.yml` (inherited from upstream).
- No installer signing has ever happened.
- No release-notes file or CHANGELOG.
- No version-bump checklist in docs.

## 4. Tag conventions

Match the reference repos exactly:

| Tag pattern | Action |
|---|---|
| `v<major>.<minor>.<patch>` | Full release: build all platforms, sign, notarize macOS, publish draft GitHub Release with all installers attached. |
| `v<major>.<minor>.<patch>-rc<n>` | Pre-release. Same pipeline; the GH Release is created with `prerelease: true`. |
| `v...-no-notarize` | Build + sign as normal but skip the macOS notarization submit/wait (faster turnaround for testing). Magic substring honored by the workflow. |
| `legacy-...` | Reserved for any future legacy-flow keep-alive (matches `metavinci`'s pattern); not used in v1. |

`workflow_dispatch` handles ad-hoc manual builds without a tag — useful for verifying changes to the workflow itself.

## 5. Build matrix (Phase 1)

Three jobs, all matching what the existing Conan profiles support:

| Job | Runner | Conan profile | Output artifact |
|---|---|---|---|
| `build-linux` | `ubuntu-22.04` | `linux-x86_64` | `Inkternity-<ver>-x86_64.flatpak` (Phase 1) |
| `build-windows` | `windows-2022` | `win-x86_64` | `Inkternity-<ver>-x64.exe` (NSIS installer) + signed |
| `build-macos` | `macos-14` (arm64) | `macOS-arm64` | `Inkternity-<ver>-arm64.dmg` + signed + notarized |

**Single-arch macOS for Phase 1**, mirroring `glasswing` (not `metavinci`'s dual-arch matrix). Intel macOS support added when there's user demand — adding the matrix axis later is a workflow YAML diff, not a code change.

**No Android, no Linux arm64, no web build in Phase 1.** Both deferred — see §10.

## 6. Per-platform packaging

### 6.1 Linux: Flatpak

Inkternity already has `linuxinstall/com.inkternity.inkternity.{yml,metainfo.xml,desktop}` — the Flatpak manifest. The build job:

1. Installs `flatpak`, `flatpak-builder`, the freedesktop runtime + SDK (the manifest specifies `org.freedesktop.Platform 25.08` per the existing yml).
2. Builds via `flatpak-builder --repo=repo build-dir linuxinstall/com.inkternity.inkternity.yml`.
3. Bundles via `flatpak build-bundle repo Inkternity-<ver>-x86_64.flatpak com.inkternity.inkternity`.

**Why Flatpak over .deb**: Flatpak is distro-agnostic (works on Ubuntu/Fedora/Arch/etc.), the manifest is already committed, and the metainfo XML enables flathub publishing as a future option. Reference repos use `.deb` for Debian/Ubuntu users, but those projects ship Python apps that don't need cross-distro compatibility the way a C++ + Skia + libdatachannel app does.

If the user wants `.deb` too as a follow-up, it's an additional packaging step — we'd add it without removing Flatpak.

### 6.2 Windows: NSIS via CPack

`BUILDING.md` confirms `cpack -G NSIS` works locally. The build job:

1. Installs MSVC 2022 build tools (`microsoft/setup-msbuild@v2`).
2. Runs the `BUILDING.md` Windows recipe verbatim (`conan install ...`, `cmake --build ...`).
3. `cpack -G NSIS` produces `Inkternity-<ver>-win64.exe`.
4. **Code signing step**:
   ```yaml
   - name: Decode + import signing cert
     run: |
       $bytes = [Convert]::FromBase64String("${{ secrets.WINDOWS_SIGNING_CERT }}")
       [IO.File]::WriteAllBytes("$env:RUNNER_TEMP\inkternity-cert.pfx", $bytes)
   - name: Sign installer
     run: |
       & 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.*\x64\signtool.exe' sign /f "$env:RUNNER_TEMP\inkternity-cert.pfx" /p "${{ secrets.WINDOWS_SIGNING_PASSWORD }}" /tr http://timestamp.digicert.com /td sha256 /fd sha256 Inkternity-*.exe
   ```
5. Hard-fail on signing failure (mirror `metavinci`, not `glasswing`'s soft-fail). Public-distribution installers should never silently ship unsigned.

### 6.3 macOS: DragNDrop DMG + notarization

`BUILDING.md` confirms `cpack -G DragNDrop` works locally. The build job:

1. `xcode-select` already on the runner.
2. Standard Conan + CMake build per `BUILDING.md` macOS recipe.
3. `cpack -G DragNDrop` produces `Inkternity-<ver>-arm64.dmg` containing the `.app` bundle.
4. **Code signing**:
   - Create temp keychain.
   - Import `MACOS_APPLICATION_P12` (decoded from base64) using `MACOS_CERT_PW`.
   - Sign the `.app`: `codesign --options runtime --timestamp --deep --sign "<identity hash>" Inkternity.app`.
   - Sign the DMG.
5. **Notarization** (skipped if tag contains `-no-notarize` OR `workflow_dispatch.inputs.skip-notarize == true`):
   - `xcrun notarytool submit Inkternity-*.dmg --apple-id $APPLE_ID --team-id $APPLE_TEAM_ID --password $APPLE_ID_APP_PW --wait` (timeout 1200s like the reference repos).
   - `xcrun stapler staple Inkternity-*.dmg`.
6. Hard-fail on signing/notarization failure.

The `environment: installers` selector on the job gates secret access (matches `metavinci`).

## 7. Versioning + release notes workflow

### 7.1 Source-of-truth flow

```
git tag v0.12.0 → workflow triggered → BUILD_VERSION=v0.12.0 → installer metadata
                                                              ↓
                                                       0.12.0 (numeric strip)
```

- `VersionConstants.hpp` (`CURRENT_VERSION_STRING` / `CURRENT_VERSION_NUMBER` / `CURRENT_SAVEFILE_HEADER`) is the **file-format** version, NOT the release version. Bumped independently when the format changes. Don't auto-update from tag — would produce false positives in version-gated load paths.
- Installer metadata (Windows MSI ProductVersion, macOS Info.plist `CFBundleVersion`, Flatpak metainfo) IS auto-derived from the tag.

### 7.2 Release notes

Phase 1: hand-written `body` field in the workflow's `softprops/action-gh-release@v1` step, like the reference repos. Brief, structured, mentions:

- What's new (1-3 bullets).
- Anything users need to do (e.g., format upgrade behavior).
- Known issues.
- Per-platform install instructions / requirements.

Phase 1.5 (when there's enough release cadence to justify it): introduce `CHANGELOG.md` + parse the relevant version's section into the release body. Both reference repos have NOT adopted this yet — Inkternity follows their lead.

### 7.3 The release helper script

`scripts/create_release.py` mirrors `metavinci/create_release.py` and `glasswing/scripts/create_release.py`:

```python
# pseudocode
last = run(["git", "tag", "--sort=-version:refname"]).split()[0]
suggested = bump_patch(last)
print(f"Last release: {last}")
print(f"Suggested next: {suggested}")
# Optional: run `git tag <suggested> && git push origin <suggested>` on confirmation.
```

Optional but very useful — without it the release dance is "go look at GitHub releases, pick a number, hope nobody collides." With it, one-command release.

## 8. Code signing strategy

### 8.1 Inheritance from existing infra

HEAVYMETA already owns the Authenticode cert (`heavymeta-code-sign` / `andromica-code-sign` PFXs are stored in the same secret pair, reused across `metavinci` and `glasswing`) and the Apple Developer Program account (used for `glasswing`'s Andromica notarization). Inkternity reuses these — **no new certs need provisioning**.

### 8.2 What ships per-platform

| Platform | Signed | Notarized | Cert source |
|---|---|---|---|
| Windows NSIS installer | ✓ (signtool, DigiCert RFC3161 timestamp) | n/a | `WINDOWS_SIGNING_CERT` (existing) |
| macOS DMG | ✓ (codesign hardened runtime) | ✓ (xcrun notarytool) | `MACOS_APPLICATION_P12` (existing) |
| Linux Flatpak | n/a (Flatpak repo signing is its own thing) | n/a | — |

### 8.3 Hard-fail policy

If signing or notarization fails on a real release tag (not `-no-notarize`), the job fails and no GitHub Release is created. Mirrors `metavinci`. The only acceptable unsigned shipping case is `workflow_dispatch` builds (devs explicitly choosing to skip).

## 9. Publishing

- `create-release` job: `needs: [build-linux, build-windows, build-macos]`, `if: startsWith(github.ref, 'refs/tags/')`.
- Downloads all three platform artifacts via `actions/download-artifact@v4`.
- Calls `softprops/action-gh-release@v1` with:
  - `tag_name: ${{ github.ref_name }}`
  - `draft: true`
  - `prerelease: ${{ contains(github.ref_name, '-rc') || contains(github.ref_name, '-beta') }}`
  - `files: release/*` (the gathered artifacts)
  - `body: |` (hand-written, expanded each release)
- Default `GITHUB_TOKEN` permissions (`contents: write`).

Why `draft: true`: matches reference repos. Lets the maintainer review the release body, attach screenshots, fix typos before publishing publicly. The "publish" step is one click in the GitHub UI after the workflow completes.

## 10. Out of scope for Phase 1

- **Android build.** `android-project/` exists but neither reference repo has an Android pipeline to copy from. Phase 2 work — needs Gradle workflow, signing keystore in a new secret, optional Play Store publishing decision.
- **Linux arm64.** Single x86_64 only for now; matches reference repos.
- **macOS Intel.** Apple Silicon only for Phase 1 (mirrors `glasswing`). Add the dual-arch matrix later if Intel demand surfaces; the `metavinci` workflow shows the pattern.
- **Web / Emscripten build.** `emscripteninstall/` HTML shells exist but there's no precedent for static-site publishing in either reference repo. Defer; could publish to GitHub Pages or a CDN as a separate workflow.
- **CHANGELOG.md.** Not adopted by either reference repo. Defer until release cadence demands it.
- **HEAVYMETA-specific build steps.** No precedent: no Stellar key injection at build, no IPFS pinning of releases, no NFC card flow, no cooperative-membership gate on the pipeline. Both reference workflows are vanilla GitHub Actions in this respect. Inkternity follows suit.
- **Auto-update mechanism.** Neither reference repo ships an auto-updater. Manual download + reinstall remains the user flow.

## 11. Subtask breakdown

| | Deliverable |
|---|---|
| B1 | Create `.github/workflows/build-installers.yml` skeleton with the three build jobs + create-release. Trigger on `v*` tags + `workflow_dispatch`. |
| B2 | `build-linux` job: Conan install, CMake build, Flatpak bundle. Upload artifact. |
| B3 | `build-windows` job: Conan install, MSVC build, CPack NSIS, signtool. Upload artifact. Hard-fail on signing failure. |
| B4 | `build-macos` job: Conan install, CMake build, CPack DragNDrop, codesign + notarize (skip notarize on `-no-notarize` substring or `workflow_dispatch.skip-notarize`). Upload artifact. Hard-fail. |
| B5 | `create-release` job: download artifacts, softprops/action-gh-release@v1 draft release with hand-written body. |
| B6 | `scripts/create_release.py` helper — last-tag inspection, next-tag suggestion. Mirror `metavinci`'s. |
| B7 | `docs/BUILDING.md` updates: reference the workflow, document the version-bump checklist (when to bump VersionConstants vs when the tag suffices), `-no-notarize` convention, manual-dispatch usage. |
| B8 | Add `RELEASE_CHECKLIST.md` (or extend BUILDING.md): the steps a maintainer takes before tagging — bump VersionConstants if format changed, run a local smoke build, draft release notes, run `create_release.py`. |
| B9 | First end-to-end test: tag `v0.11.0-rc1-no-notarize`, verify all three jobs build artifacts, draft release appears with three installers. |
| B10 | Second end-to-end test with notarization on: tag `v0.11.0-rc2`, verify macOS notarization completes within 20 min and the stapled DMG installs cleanly on a fresh mac. |
| B11 | First real release: tag `v0.11.0`. Promote draft release to published in the GitHub UI. |

B1–B5 are roughly **3–4 days** of YAML / shell work for someone who's done a GitHub Actions release before. B9–B10 are slow-feedback steps (each tag-push round-trip is the full pipeline runtime, ~15–30 min depending on coturn-style native compile times). Realistic total elapsed: **1 week** with iteration time accounted for.

## 12. Open questions before starting

- **Existing signing certs valid for Inkternity binary metadata?** The Authenticode subject and Apple notarization team are tied to HEAVYMETA's existing apps. Confirm Inkternity is OK to ship under the same signer identity (it should be — it's the same cooperative). If a separate signer identity is preferred for branding, that's a new cert provisioning task.
- **Flatpak vs deb (or both)?** Recommended Flatpak (already has manifest, distro-agnostic). If `.deb` is needed for any reason, add as a parallel artifact in the Linux job — not exclusive.
- **GitHub repository for the build?** Inkternity's GitHub remote (currently the `zynx/inkternity` placeholder per other docs in this dir) needs to actually exist with the secrets configured. Not a code task but a prerequisite.
- **Notarization environment selector.** `metavinci` uses `environment: installers` to gate access to the macOS secrets behind a GitHub Environment with required reviewers. Adopt? (Recommended yes — operational consistency.)
- **First version to ship through this pipeline.** `VersionConstants.hpp` currently says `0.11.0`. The first tag could be `v0.11.0` (matches) — but only if the format is genuinely stable at this version. Otherwise pick `v0.11.0-rc1` to signal that this is the first GA-track release.
- **Does the upstream InfiniPaint dev's signing infrastructure transfer?** No — they use their own code-signing setup. Inkternity is a fork; HEAVYMETA's certs are unrelated.

---

## Appendix A: Workflow skeleton (illustrative)

```yaml
# .github/workflows/build-installers.yml
name: Build installers

on:
  push:
    tags: [ 'v*' ]
    paths-ignore: ['**.md', 'docs/**']
  workflow_dispatch:
    inputs:
      skip-notarize:
        description: 'Skip macOS notarization'
        type: boolean
        default: false

env:
  BUILD_VERSION: ${{ github.ref_name }}

jobs:
  build-linux:
    runs-on: ubuntu-22.04
    steps:
      # checkout, conan install, cmake build, flatpak bundle, upload-artifact
      ...

  build-windows:
    runs-on: windows-2022
    steps:
      # checkout, conan install, cmake build, cpack NSIS, signtool, upload-artifact
      ...

  build-macos:
    runs-on: macos-14
    environment: installers
    steps:
      # checkout, conan install, cmake build, cpack DragNDrop,
      # codesign, notarize-unless-skip, staple, upload-artifact
      ...

  create-release:
    needs: [ build-linux, build-windows, build-macos ]
    if: startsWith(github.ref, 'refs/tags/')
    runs-on: ubuntu-22.04
    permissions:
      contents: write
    steps:
      - uses: actions/download-artifact@v4
        with: { path: release }
      - uses: softprops/action-gh-release@v1
        with:
          tag_name: ${{ github.ref_name }}
          draft: true
          prerelease: ${{ contains(github.ref_name, '-rc') || contains(github.ref_name, '-beta') }}
          files: release/**
          body: |
            ## Inkternity ${{ github.ref_name }}

            <hand-written notes>
```

## Appendix B: How this slots alongside the existing HEAVYMETA repos

| Repo | Build tool | Installer formats | Active workflow |
|---|---|---|---|
| `metavinci` | Nuitka (Python) | WiX MSI, dpkg deb, hdiutil DMG | `build-nuitka.yml` |
| `glasswing` (Andromica) | PyInstaller (Python) | WiX MSI, dpkg deb, hdiutil DMG | `build-installers.yml` |
| **`inkternity`** (this) | CMake + Conan (C++) | NSIS exe, Flatpak, DragNDrop DMG | `build-installers.yml` (proposed) |

Pipeline shape, secret names, tag conventions, signing/notarization flow, draft-release pattern: identical across all three. Build tooling differs because the apps are written in different languages — that's the only meaningful divergence and it's unavoidable.
