#!/usr/bin/env python3
"""
Regenerate every platform-specific icon derivative from the master SVG.

Master input:
  assets/data/progicons/icon_SRC.svg

Outputs (all relative to the repo root):
  assets/data/progicons/icon.png                                    (64x64, in-app SDL window icon)
  logo.svg                                                          (Linux/Flatpak SVG copy)
  windowsinstall/icon.ico                                           (multi-size ICO: 16/32/48/64/128/256)
  macosinstall/appicon.icns                                         (macOS .app bundle icon)
  android-project/app/src/main/res/mipmap-mdpi/ic_launcher.png      (48x48)
  android-project/app/src/main/res/mipmap-hdpi/ic_launcher.png      (72x72)
  android-project/app/src/main/res/mipmap-xhdpi/ic_launcher.png     (96x96)
  android-project/app/src/main/res/mipmap-xxhdpi/ic_launcher.png    (144x144)
  android-project/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png   (192x192)

Requires ImageMagick (`magick` CLI) on PATH. Idempotent — safe to re-run.
Mirrors the manual sequence used in commits a3b1dea ("icons: sync ICO/ICNS/
Flatpak SVG to current icon_SRC.svg") and b399d26 ("android: regenerate
launcher icons from current SVG"). Run after editing icon_SRC.svg.

Usage (from repo root):
  uv run scripts/regen_icons.py
  uv run scripts/regen_icons.py --check   # just verify magick is installed
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MASTER = REPO_ROOT / "assets" / "data" / "progicons" / "icon_SRC.svg"

# (relative-output-path, square pixel size). Order is purely for log readability.
PNG_TARGETS: list[tuple[str, int]] = [
    ("assets/data/progicons/icon.png", 64),
    ("android-project/app/src/main/res/mipmap-mdpi/ic_launcher.png", 48),
    ("android-project/app/src/main/res/mipmap-hdpi/ic_launcher.png", 72),
    ("android-project/app/src/main/res/mipmap-xhdpi/ic_launcher.png", 96),
    ("android-project/app/src/main/res/mipmap-xxhdpi/ic_launcher.png", 144),
    ("android-project/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png", 192),
]

ICO_TARGET = "windowsinstall/icon.ico"
# Sizes Windows expects in a single multi-image ICO (matches Explorer's tile/
# detail/thumbnail render paths). The auto-resize define produces all of them
# in one ICO container.
ICO_SIZES = "256,128,64,48,32,16"

ICNS_TARGET = "macosinstall/appicon.icns"
# ICNS holds multiple resolutions internally; rendering at 1024 lets ImageMagick
# build the standard set (16, 32, 128, 256, 512 + retina @2x).
ICNS_RENDER_SIZE = 1024

SVG_COPY_TARGET = "logo.svg"


def have_magick() -> bool:
    return shutil.which("magick") is not None


def run_magick(args: list[str], label: str) -> bool:
    cmd = ["magick", *args]
    print(f"  {label}")
    print(f"    $ {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"    ERROR (exit {result.returncode}): {result.stderr.strip() or result.stdout.strip()}",
              file=sys.stderr)
        return False
    return True


def render_png(size: int, dest: Path) -> bool:
    dest.parent.mkdir(parents=True, exist_ok=True)
    return run_magick(
        [str(MASTER), "-background", "none", "-resize", f"{size}x{size}", str(dest)],
        f"-> {dest.relative_to(REPO_ROOT)}  ({size}x{size})",
    )


def render_ico(dest: Path) -> bool:
    dest.parent.mkdir(parents=True, exist_ok=True)
    return run_magick(
        [str(MASTER), "-define", f"icon:auto-resize={ICO_SIZES}", str(dest)],
        f"-> {dest.relative_to(REPO_ROOT)}  (sizes {ICO_SIZES})",
    )


def render_icns(dest: Path) -> bool:
    dest.parent.mkdir(parents=True, exist_ok=True)
    return run_magick(
        [str(MASTER), "-resize", f"{ICNS_RENDER_SIZE}x{ICNS_RENDER_SIZE}", str(dest)],
        f"-> {dest.relative_to(REPO_ROOT)}  (rendered at {ICNS_RENDER_SIZE}x{ICNS_RENDER_SIZE})",
    )


def copy_svg(dest: Path) -> bool:
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"  -> {dest.relative_to(REPO_ROOT)}  (copy of master)")
    shutil.copy2(MASTER, dest)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Regenerate platform icons from icon_SRC.svg")
    parser.add_argument("--check", action="store_true", help="Verify magick is on PATH and exit")
    args = parser.parse_args()

    if not have_magick():
        print("ERROR: ImageMagick `magick` not on PATH.", file=sys.stderr)
        print("       Install from https://imagemagick.org/script/download.php#windows", file=sys.stderr)
        return 1

    if args.check:
        print("magick OK")
        return 0

    if not MASTER.exists():
        print(f"ERROR: master SVG not found at {MASTER}", file=sys.stderr)
        return 1

    print(f"Master: {MASTER.relative_to(REPO_ROOT)}")
    print()

    failures: list[str] = []

    print("PNG outputs:")
    for rel, size in PNG_TARGETS:
        if not render_png(size, REPO_ROOT / rel):
            failures.append(rel)
    print()

    print("ICO output:")
    if not render_ico(REPO_ROOT / ICO_TARGET):
        failures.append(ICO_TARGET)
    print()

    print("ICNS output:")
    if not render_icns(REPO_ROOT / ICNS_TARGET):
        failures.append(ICNS_TARGET)
    print()

    print("SVG copy:")
    if not copy_svg(REPO_ROOT / SVG_COPY_TARGET):
        failures.append(SVG_COPY_TARGET)
    print()

    if failures:
        print(f"FAILED: {len(failures)} target(s)", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1

    print("All icon outputs regenerated. Review with `git status` before committing.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
