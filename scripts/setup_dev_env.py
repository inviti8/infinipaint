#!/usr/bin/env python3
"""
Set up the user-level environment variables Inkternity development needs.

Auto-detects installed toolchains (Android SDK, NDK, JDK, Conan) on this
Windows machine, proposes values, and — with --apply — persists them via
`setx`. Dry-run by default, mirroring the convention of create_release.py
/ inject_turn_secret.py / prune_actions_artifacts.py in this scripts/
directory.

Variables managed:

  ANDROID_HOME       Android SDK root (e.g. D:\\android-sdk)
  ANDROID_SDK_ROOT   Same as ANDROID_HOME (legacy alias gradle/some tools want)
  ANDROID_NDK_HOME   Specific NDK install matching the version pinned in
                     android-project/app/build.gradle's `ndkVersion` field
  JAVA_HOME          JDK 17 install root (Adoptium Temurin or Android
                     Studio's bundled JBR)

Variables verified (reported, not set — they should already exist or are
set elsewhere):

  CONAN_HOME         User memory says D:\\.conan2 — checked, not modified
  TURN_SECRET        Sourced from .env at runtime by inject_turn_secret.py;
                     never goes into env vars (would leak to subprocesses)

PATH is intentionally NOT modified. Adjusting PATH via setx is fragile
(can truncate, can lose entries) and platform-tools / sdkmanager are
better invoked via full path or a small per-shell prepend you control.

Usage:

  # Dry-run: detect everything, show what would change.
  uv run --no-project python scripts/setup_dev_env.py

  # Persist via setx. New cmd / pwsh / Studio sessions will see the values
  # — already-running sessions need a restart.
  uv run --no-project python scripts/setup_dev_env.py --apply
"""

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


REPO_ROOT = Path(__file__).resolve().parent.parent
GRADLE_FILE = REPO_ROOT / "android-project" / "app" / "build.gradle"


@dataclass
class EnvVar:
    name: str
    current: Optional[str]   # what's set right now (None if unset)
    proposed: Optional[str]  # what we'd set it to (None if can't detect)
    note: str                # human-readable explanation


# ---------- detection ----------

def detect_android_sdk() -> Optional[Path]:
    """Look in the common Windows install spots, return first match."""
    candidates = [
        os.environ.get("ANDROID_HOME"),
        os.environ.get("ANDROID_SDK_ROOT"),
        rf"{os.environ.get('LOCALAPPDATA', '')}\Android\Sdk",
        r"D:\AndroidStudio",        # this repo's convention
        r"D:\android-sdk",
        r"C:\android-sdk",
        rf"{Path.home()}\AppData\Local\Android\Sdk",
    ]
    for c in candidates:
        if not c:
            continue
        p = Path(c)
        # Real SDK has at least platform-tools/ + a platforms/ dir
        if p.is_dir() and (p / "platform-tools").is_dir():
            return p
    return None


def required_ndk_version() -> Optional[str]:
    """Parse the ndkVersion pinned in android-project/app/build.gradle."""
    if not GRADLE_FILE.exists():
        return None
    text = GRADLE_FILE.read_text(encoding="utf-8")
    m = re.search(r'ndkVersion\s*=\s*["\']([^"\']+)["\']', text)
    return m.group(1) if m else None


def detect_android_ndk(sdk: Optional[Path], required: Optional[str]) -> Optional[Path]:
    """Find the NDK whose folder name matches the gradle-pinned version."""
    if not sdk or not required:
        return None
    ndk_root = sdk / "ndk" / required
    return ndk_root if ndk_root.is_dir() else None


def detect_java() -> Optional[Path]:
    """Find a JDK >= 17. Prefer Adoptium Temurin, fall back to Studio JBR."""
    candidates = []

    # Adoptium Temurin (winget install EclipseAdoptium.Temurin.17.JDK)
    pf = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    adoptium = pf / "Eclipse Adoptium"
    if adoptium.is_dir():
        # jdk-17.0.x-hotspot — sort to pick highest
        for jdk in sorted(adoptium.glob("jdk-17.*"), reverse=True):
            if (jdk / "bin" / "java.exe").is_file():
                candidates.append(jdk)

    # Android Studio bundled JBR (often Java 21 in modern Studio, still works)
    studio_jbr = pf / "Android" / "Android Studio" / "jbr"
    if (studio_jbr / "bin" / "java.exe").is_file():
        candidates.append(studio_jbr)

    # Already-set JAVA_HOME if it points somewhere real
    cur = os.environ.get("JAVA_HOME")
    if cur and (Path(cur) / "bin" / "java.exe").is_file():
        candidates.append(Path(cur))

    return candidates[0] if candidates else None


def detect_conan() -> tuple[Optional[Path], Optional[str]]:
    """Return (conan_home, conan_binary_path). Both may be None."""
    home = os.environ.get("CONAN_HOME")
    home_path = Path(home) if home else None
    if home_path and not home_path.is_dir():
        home_path = None

    # Find conan binary via `where conan` (Windows equivalent of `which`)
    binary = None
    try:
        out = subprocess.run(["where", "conan"], capture_output=True, text=True, check=False)
        if out.returncode == 0:
            first = out.stdout.strip().splitlines()[0] if out.stdout.strip() else ""
            if first and Path(first).is_file():
                binary = first
    except FileNotFoundError:
        pass

    return home_path, binary


def check_turn_secret() -> tuple[bool, str]:
    """Verify .env contains TURN_SECRET. Don't print its value."""
    env_path = REPO_ROOT / ".env"
    if not env_path.exists():
        return False, ".env not present (no TURN_SECRET — flatpak/installer builds will ship the placeholder)"
    text = env_path.read_text(encoding="utf-8")
    if "TURN_SECRET=" in text:
        return True, ".env present, contains TURN_SECRET"
    return False, ".env present but TURN_SECRET key missing"


# ---------- setx ----------

def setx_user(name: str, value: str) -> bool:
    """Persist as a user-level env var. Returns True on success."""
    r = subprocess.run(["setx", name, value], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"  ERROR: setx {name} failed: {r.stderr.strip()}", file=sys.stderr)
        return False
    return True


# ---------- output ----------

def fmt_value(v: Optional[str], maxlen: int = 60) -> str:
    if v is None:
        return "(unset)"
    s = str(v)
    return s if len(s) <= maxlen else s[: maxlen - 3] + "..."


def print_table(rows: list[EnvVar]) -> None:
    name_w = max(len(r.name) for r in rows)
    print(f"  {'Var':<{name_w}}  {'Current':<60}  {'Proposed':<60}  Action")
    print(f"  {'-'*name_w}  {'-'*60}  {'-'*60}  ------")
    for r in rows:
        action = (
            "skip (unchanged)"   if r.current == r.proposed and r.current is not None
            else "SET"           if r.proposed and r.current != r.proposed
            else "no candidate"  if r.proposed is None
            else "?"
        )
        print(f"  {r.name:<{name_w}}  {fmt_value(r.current):<60}  {fmt_value(r.proposed):<60}  {action}")


# ---------- main ----------

def main() -> int:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--apply", action="store_true",
                   help="Actually persist via setx (default: dry-run print only).")
    args = p.parse_args()

    sdk = detect_android_sdk()
    required_ndk = required_ndk_version()
    ndk = detect_android_ndk(sdk, required_ndk)
    java = detect_java()
    conan_home, conan_bin = detect_conan()
    turn_ok, turn_msg = check_turn_secret()

    rows: list[EnvVar] = [
        EnvVar("ANDROID_HOME",
               os.environ.get("ANDROID_HOME"),
               str(sdk) if sdk else None,
               "Android SDK root"),
        EnvVar("ANDROID_SDK_ROOT",
               os.environ.get("ANDROID_SDK_ROOT"),
               str(sdk) if sdk else None,
               "Legacy alias for ANDROID_HOME — gradle / some tools want it"),
        EnvVar("ANDROID_NDK_HOME",
               os.environ.get("ANDROID_NDK_HOME"),
               str(ndk) if ndk else None,
               f"NDK matching app/build.gradle ndkVersion={required_ndk or '?'}"),
        EnvVar("JAVA_HOME",
               os.environ.get("JAVA_HOME"),
               str(java) if java else None,
               "JDK >= 17 for AGP 8.7+"),
    ]

    print()
    print("=== Detected toolchains ===")
    print(f"  Android SDK  : {sdk or '(not found)'}")
    print(f"  NDK ver req. : {required_ndk or '(could not parse build.gradle)'}")
    print(f"  NDK install  : {ndk or '(not found at $SDK/ndk/<ver>)'}")
    print(f"  Java JDK     : {java or '(not found in Adoptium / Studio JBR)'}")
    print(f"  Conan home   : {conan_home or '(unset)'}")
    print(f"  Conan binary : {conan_bin or '(not on PATH)'}")
    print(f"  TURN secret  : {turn_msg}")
    print()
    print("=== Environment variables ===")
    print_table(rows)
    print()

    # Warn on missing prerequisites
    warnings: list[str] = []
    if not sdk:
        warnings.append(
            "Android SDK not found in any standard location. Install Android Studio "
            "(`winget install Google.AndroidStudio`) or set ANDROID_HOME by hand to "
            "where you installed it.")
    if sdk and required_ndk and not ndk:
        warnings.append(
            f"NDK version {required_ndk} (pinned in app/build.gradle) is not installed. "
            f"Install via Android Studio: SDK Manager > SDK Tools > NDK (Side by Side) > "
            f"check 'Show Package Details' > pick {required_ndk}.")
    if not java:
        warnings.append(
            "JDK 17 not found. Install with: winget install EclipseAdoptium.Temurin.17.JDK")
    if not conan_bin:
        warnings.append(
            "conan binary not on PATH. You probably run it via `uv run --no-project conan`. "
            "If gradle's conanInstall task ever needs to find it directly, set "
            "`conanExe=...` in android-project/gradle.properties (see app/build.gradle).")
    if not turn_ok:
        warnings.append(
            f"TURN_SECRET issue: {turn_msg}. Builds will produce installers with the "
            f"`__TURN_SECRET__` placeholder — TURN won't authenticate at runtime, but "
            f"P2P + STUN still work for testing.")
    if warnings:
        print("=== Warnings ===")
        for w in warnings:
            print(f"  * {w}")
        print()

    # Decide what to actually do
    to_set = [r for r in rows if r.proposed and r.current != r.proposed]

    if not to_set:
        print("Nothing to set — all detected vars already match the proposed values.")
        return 0

    if not args.apply:
        print("Dry run. Re-run with --apply to persist via setx.")
        print("(setx writes to user-level env. Open a NEW terminal / restart Studio")
        print(" to see the new values; already-running shells won't pick them up.)")
        return 0

    print(f"Persisting {len(to_set)} variable(s) via setx...")
    failed = 0
    for r in to_set:
        if setx_user(r.name, r.proposed):
            print(f"  set {r.name} = {fmt_value(r.proposed)}")
        else:
            failed += 1
    print()
    if failed:
        print(f"DONE with {failed} failures. Open a new terminal to see successful changes.")
        return 1
    print("DONE. Open a new terminal / restart Android Studio to pick up the changes.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
