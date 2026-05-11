#!/usr/bin/env python3
"""
Release-helper script for Inkternity.

Mirrors metavinci/create_release.py and glasswing/scripts/create_release.py
so the HEAVYMETA "tag, push, release" rhythm is the same across repos.

  Last  release: v0.11.0
  Suggest next:  v0.11.1   (patch bump by default)

Pass --minor or --major to bump the corresponding axis instead, or pass
an explicit --tag if you want to skip the suggestion.

By default the script PRINTS the suggested commands but does not run
them — pass --push to actually create + push the tag, which triggers
the GitHub Actions release workflow on the remote.

Examples:

  # Just suggest the next tag, print the commands to run manually.
  python scripts/create_release.py

  # Patch-bump, create local tag, push to origin -> triggers CI.
  python scripts/create_release.py --push

  # Minor-bump pre-release.
  python scripts/create_release.py --minor --tag v0.12.0-rc1 --push

  # Skip macOS notarization for a faster turnaround on testing builds.
  python scripts/create_release.py --tag v0.12.0-rc1-no-notarize --push
"""

import argparse
import re
import subprocess
import sys
from typing import Optional

SEMVER_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)(?:-.+)?$")


def latest_tag() -> Optional[str]:
    """Return the most-recent v* tag by version sort, or None if none exist."""
    try:
        out = subprocess.run(
            ["git", "tag", "--sort=-version:refname", "--list", "v*"],
            capture_output=True, text=True, check=True,
        ).stdout.strip().splitlines()
    except subprocess.CalledProcessError:
        return None
    for line in out:
        if SEMVER_RE.match(line):
            return line
    return None


def parse(tag: str) -> tuple[int, int, int]:
    m = SEMVER_RE.match(tag)
    if not m:
        raise ValueError(f"not a recognized vX.Y.Z tag: {tag}")
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def bump(prev: Optional[str], part: str) -> str:
    if prev is None:
        # First release ever.
        return "v0.1.0"
    maj, minr, pat = parse(prev)
    if   part == "major": return f"v{maj+1}.0.0"
    elif part == "minor": return f"v{maj}.{minr+1}.0"
    else:                 return f"v{maj}.{minr}.{pat+1}"


def main() -> None:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("--major", action="store_const", const="major", dest="part",
                   help="Bump the major version (X.0.0)")
    p.add_argument("--minor", action="store_const", const="minor", dest="part",
                   help="Bump the minor version (x.Y.0)")
    p.add_argument("--patch", action="store_const", const="patch", dest="part",
                   default="patch",
                   help="Bump the patch version (x.y.Z) — default")
    p.add_argument("--tag", metavar="TAG",
                   help="Use TAG verbatim instead of bumping. Useful for "
                        "pre-release suffixes or the -no-notarize convention.")
    p.add_argument("--push", action="store_true",
                   help="Actually create the local tag and push to origin "
                        "(default: just print the commands).")
    p.add_argument("--remote", default="origin",
                   help="Remote name to push to (default: origin).")
    args = p.parse_args()

    prev = latest_tag()
    suggested = args.tag or bump(prev, args.part)

    print(f"Last release:    {prev or '(none)'}")
    print(f"Suggested next:  {suggested}")
    print()

    if not args.push:
        print("To create + push the tag, run:")
        print(f"    git tag {suggested}")
        print(f"    git push {args.remote} {suggested}")
        print()
        print("Or re-run this script with --push.")
        return

    # Validate before touching anything
    try:
        parse(suggested.split("-", 1)[0] if "-" in suggested else suggested)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(2)

    # Confirm we're on a clean tree
    status = subprocess.run(
        ["git", "status", "--porcelain"], capture_output=True, text=True, check=True,
    ).stdout.strip()
    if status:
        print("ERROR: working tree is not clean. Commit or stash first.", file=sys.stderr)
        print(status, file=sys.stderr)
        sys.exit(2)

    # Confirm the tag doesn't already exist
    existing = subprocess.run(
        ["git", "tag", "--list", suggested], capture_output=True, text=True, check=True,
    ).stdout.strip()
    if existing:
        print(f"ERROR: tag {suggested} already exists.", file=sys.stderr)
        sys.exit(2)

    subprocess.run(["git", "tag", suggested], check=True)
    subprocess.run(["git", "push", args.remote, suggested], check=True)
    print(f"Tag {suggested} created and pushed. "
          f"Watch the workflow at https://github.com/<org>/<repo>/actions")


if __name__ == "__main__":
    main()
