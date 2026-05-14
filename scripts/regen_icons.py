#!/usr/bin/env python3
# /// script
# dependencies = ["icnsutil"]
# ///
"""
Regenerate every platform-specific icon derivative from the master SVG.

Pipeline (two-stage to keep line-weight consistent across sizes):
  1. Rasterize icon_SRC.svg ONCE at 2048x2048 -> master PNG (in a temp dir).
     Letting magick rasterize the SVG independently at each target size
     produces visually inconsistent stroke widths — a 2% stroke renders as
     a hard ~1px line at 48x48 but as a smooth ~20px line at 1024x1024,
     and they aren't the same shape, just smaller/larger. Rasterizing once
     and resampling makes every derivative a faithful reduction of the
     same master render.
  2. All raster derivatives (PNG/ICO/ICNS) are Lanczos-resampled from the
     master PNG, preserving the SVG's alpha channel (-background none).

ICO is built as a true multi-image container by feeding magick all the
per-size PNGs at once, rather than letting -define icon:auto-resize
re-rasterize the SVG (which loses transparency in many ImageMagick builds
and re-introduces the per-size line-weight problem inside the container).

ICNS is built via the `icnsutil` Python package — ImageMagick's ICNS coder
only writes a single frame even when given multiple PNGs (silently keeps
the first/last depending on build), producing a useless 16x16 .icns.
icnsutil packs proper multi-resolution containers with retina variants.
The PEP 723 inline metadata above pins it so `uv run` auto-installs.

Master input:
  assets/data/progicons/icon_SRC.svg

Outputs (all relative to the repo root):
  assets/data/progicons/icon.png                                    (64x64, in-app SDL window icon)
  logo.svg                                                          (Linux/Flatpak SVG copy)
  windowsinstall/icon.ico                                           (multi-image: 16/32/48/64/128/256)
  macosinstall/appicon.icns                                         (multi-image: 16/32/128/256/512/1024)
  android-project/app/src/main/res/mipmap-mdpi/ic_launcher.png      (48x48)
  android-project/app/src/main/res/mipmap-hdpi/ic_launcher.png      (72x72)
  android-project/app/src/main/res/mipmap-xhdpi/ic_launcher.png     (96x96)
  android-project/app/src/main/res/mipmap-xxhdpi/ic_launcher.png    (144x144)
  android-project/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png   (192x192)

Requires ImageMagick (`magick` CLI) on PATH. Idempotent — safe to re-run.

Usage (from repo root):
  uv run scripts/regen_icons.py
  uv run scripts/regen_icons.py --check    # just verify magick is installed
  uv run scripts/regen_icons.py --keep-master  # leave master raster in tmp for inspection
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MASTER_SVG = REPO_ROOT / "assets" / "data" / "progicons" / "icon_SRC.svg"

# One-time rasterization size. Large enough that every downstream resample
# (including 1024 for ICNS retina) is a downsample, never an upscale.
MASTER_RASTER_SIZE = 2048

PNG_TARGETS: list[tuple[str, int]] = [
    ("assets/data/progicons/icon.png", 64),
    ("android-project/app/src/main/res/mipmap-mdpi/ic_launcher.png", 48),
    ("android-project/app/src/main/res/mipmap-hdpi/ic_launcher.png", 72),
    ("android-project/app/src/main/res/mipmap-xhdpi/ic_launcher.png", 96),
    ("android-project/app/src/main/res/mipmap-xxhdpi/ic_launcher.png", 144),
    ("android-project/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png", 192),
]

ICO_TARGET = "windowsinstall/icon.ico"
ICO_SIZES = [16, 32, 48, 64, 128, 256]

ICNS_TARGET = "macosinstall/appicon.icns"
# (OSType, pixel-size) pairs that we pack into the .icns. Apple icons have
# 1x and 2x retina variants, where some pixel sizes are reused by different
# OSTypes (e.g. a 32x32 PNG is icp5 for "32@1x" AND ic11 for "16@2x"),
# so icnsutil cannot auto-guess — we declare the mapping explicitly.
#
#   icp4 = 16x16        | ic11 = 16x16 @2x = 32x32 px
#   icp5 = 32x32        | ic12 = 32x32 @2x = 64x64 px
#   ic07 = 128x128      | ic13 = 128x128 @2x = 256x256 px
#   ic08 = 256x256      | ic14 = 256x256 @2x = 512x512 px
#   ic09 = 512x512      | ic10 = 512x512 @2x = 1024x1024 px
ICNS_ENTRIES: list[tuple[str, int]] = [
    ("icp4", 16),
    ("icp5", 32),
    ("ic07", 128),
    ("ic08", 256),
    ("ic09", 512),
    ("ic10", 1024),
    ("ic11", 32),
    ("ic12", 64),
    ("ic13", 256),
    ("ic14", 512),
]

SVG_COPY_TARGET = "logo.svg"


def have_magick() -> bool:
    return shutil.which("magick") is not None


def run_magick(args: list[str], label: str) -> bool:
    cmd = ["magick", *args]
    print(f"  {label}")
    print(f"    $ {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(
            f"    ERROR (exit {result.returncode}): "
            f"{result.stderr.strip() or result.stdout.strip()}",
            file=sys.stderr,
        )
        return False
    return True


def rasterize_master(dest: Path) -> bool:
    """SVG -> high-res PNG, alpha preserved, antialiased.

    -background none and -density both precede the SVG input so they
    apply to the SVG rasterization step, not the post-resize step.
    """
    return run_magick(
        [
            "-background", "none",
            "-density", "384",
            str(MASTER_SVG),
            "-resize", f"{MASTER_RASTER_SIZE}x{MASTER_RASTER_SIZE}",
            str(dest),
        ],
        f"-> {dest.name}  (SVG -> {MASTER_RASTER_SIZE}x{MASTER_RASTER_SIZE}, transparent)",
    )


def _display_path(p: Path) -> str:
    try:
        return str(p.relative_to(REPO_ROOT))
    except ValueError:
        return str(p)


def resample_png(master_png: Path, size: int, dest: Path) -> bool:
    """Downsample master PNG to size, alpha preserved, Lanczos filter."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    return run_magick(
        [
            str(master_png),
            "-background", "none",
            "-filter", "Lanczos",
            "-resize", f"{size}x{size}",
            str(dest),
        ],
        f"-> {_display_path(dest)}  ({size}x{size})",
    )


def build_ico(
    master_png: Path, sizes: list[int], dest: Path, tmp_dir: Path
) -> bool:
    """Resample master to each size, then combine into one ICO container."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    intermediates: list[Path] = []
    for size in sizes:
        intermediate = tmp_dir / f"{dest.stem}_{size}.png"
        if not resample_png(master_png, size, intermediate):
            return False
        intermediates.append(intermediate)
    if dest.exists():
        dest.unlink()
    return run_magick(
        [*[str(p) for p in intermediates], str(dest)],
        f"-> {_display_path(dest)}  (combined: {sizes})",
    )


def build_icns(
    master_png: Path,
    entries: list[tuple[str, int]],
    dest: Path,
    tmp_dir: Path,
) -> bool:
    """Resample master to each pixel size, then pack into a multi-frame ICNS.

    Uses icnsutil rather than magick because magick's ICNS coder writes
    only a single frame, producing a 16x16-only .icns that macOS can't
    render at higher resolutions. OSTypes are declared explicitly because
    pixel sizes are reused across 1x/2x OSTypes and auto-guess fails.
    """
    import icnsutil  # late import — only loaded when invoked, not at --check

    dest.parent.mkdir(parents=True, exist_ok=True)

    # Cache resampled PNGs by size so a pixel size shared by multiple
    # OSTypes (e.g. 32x32 used by both icp5 and ic11) is only rendered once.
    size_cache: dict[int, Path] = {}
    for _, size in entries:
        if size in size_cache:
            continue
        intermediate = tmp_dir / f"{dest.stem}_{size}.png"
        if not resample_png(master_png, size, intermediate):
            return False
        size_cache[size] = intermediate

    icns = icnsutil.IcnsFile()
    for ostype, size in entries:
        icns.add_media(ostype, file=str(size_cache[size]))
    if dest.exists():
        dest.unlink()
    icns.write(str(dest))
    print(
        f"  -> {_display_path(dest)}  "
        f"(icnsutil, {len(entries)} frames: {[f'{t}={s}' for t,s in entries]})"
    )
    return True


def copy_svg(dest: Path) -> bool:
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"  -> {dest.relative_to(REPO_ROOT)}  (copy of master)")
    shutil.copy2(MASTER_SVG, dest)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Regenerate platform icons from icon_SRC.svg"
    )
    parser.add_argument("--check", action="store_true",
                        help="Verify magick is on PATH and exit")
    parser.add_argument("--keep-master", action="store_true",
                        help="Persist intermediate master PNG next to icon_SRC.svg")
    args = parser.parse_args()

    if not have_magick():
        print("ERROR: ImageMagick `magick` not on PATH.", file=sys.stderr)
        print(
            "       Install from https://imagemagick.org/script/download.php#windows",
            file=sys.stderr,
        )
        return 1

    if args.check:
        print("magick OK")
        return 0

    if not MASTER_SVG.exists():
        print(f"ERROR: master SVG not found at {MASTER_SVG}", file=sys.stderr)
        return 1

    print(f"Master SVG: {MASTER_SVG.relative_to(REPO_ROOT)}")
    print()

    failures: list[str] = []

    with tempfile.TemporaryDirectory(prefix="regen_icons_") as tmp_str:
        tmp_dir = Path(tmp_str)
        master_png = tmp_dir / "icon_master.png"

        print(f"Master raster (one-time, antialiased, {MASTER_RASTER_SIZE}x{MASTER_RASTER_SIZE}):")
        if not rasterize_master(master_png):
            print("FATAL: master rasterization failed; aborting.", file=sys.stderr)
            return 1
        print()

        print("PNG outputs (resampled from master):")
        for rel, size in PNG_TARGETS:
            if not resample_png(master_png, size, REPO_ROOT / rel):
                failures.append(rel)
        print()

        print("ICO output (multi-image, resampled from master):")
        if not build_ico(master_png, ICO_SIZES, REPO_ROOT / ICO_TARGET, tmp_dir):
            failures.append(ICO_TARGET)
        print()

        print("ICNS output (icnsutil, multi-frame, resampled from master):")
        if not build_icns(master_png, ICNS_ENTRIES, REPO_ROOT / ICNS_TARGET, tmp_dir):
            failures.append(ICNS_TARGET)
        print()

        print("SVG copy:")
        if not copy_svg(REPO_ROOT / SVG_COPY_TARGET):
            failures.append(SVG_COPY_TARGET)
        print()

        if args.keep_master:
            keep_dest = MASTER_SVG.parent / f"icon_master_{MASTER_RASTER_SIZE}.png"
            shutil.copy2(master_png, keep_dest)
            print(f"Kept master raster at {keep_dest.relative_to(REPO_ROOT)}")
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
