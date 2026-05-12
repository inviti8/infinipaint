#!/usr/bin/env python3
"""
Substitute the TURN_SECRET placeholder in default_p2p.json before packaging.

The shipped TURN credential is per-deployment and must NOT be committed to
the public repo. assets/data/config/default_p2p.json carries the literal
placeholder `__TURN_SECRET__`; this script swaps it for the real secret
right before CMake/CPack bundles the assets.

Sources for the secret, in order:
  1. --secret CLI argument (CI passes ${{ secrets.TURN_SECRET }} this way)
  2. TURN_SECRET environment variable
  3. .env file in the repo root (KEY=VALUE lines, gitignored)

Behavior:
  * placeholder present + secret found  → substitute, exit 0
  * placeholder present + no secret     → leave file alone, exit 0 with warning
                                          (soft-fail: build proceeds, TURN won't
                                          authenticate, but P2P over STUN still works)
  * placeholder absent                  → idempotent no-op, exit 0
                                          (someone already substituted, fine)
  * --strict and no secret              → exit 1 (use this for release builds)

Examples:

  # Local dev — pull secret from .env, substitute in place.
  python scripts/inject_turn_secret.py

  # CI — pass secret explicitly, fail loudly if missing for a real release.
  python scripts/inject_turn_secret.py --secret "$TURN_SECRET" --strict
"""

import argparse
import os
import sys
from pathlib import Path

PLACEHOLDER = "__TURN_SECRET__"
DEFAULT_TARGET = Path("assets/data/config/default_p2p.json")


def load_dotenv(path: Path) -> dict[str, str]:
    """Minimal KEY=VALUE parser. Ignores blank lines and # comments."""
    env: dict[str, str] = {}
    if not path.exists():
        return env
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, _, v = line.partition("=")
        env[k.strip()] = v.strip().strip('"').strip("'")
    return env


def resolve_secret(args: argparse.Namespace, repo_root: Path) -> str | None:
    if args.secret:
        return args.secret
    if os.environ.get("TURN_SECRET"):
        return os.environ["TURN_SECRET"]
    return load_dotenv(repo_root / ".env").get("TURN_SECRET")


def main() -> int:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--secret", help="TURN secret string (overrides env / .env).")
    p.add_argument("--target", type=Path, default=None,
                   help=f"Path to default_p2p.json (default: {DEFAULT_TARGET})")
    p.add_argument("--strict", action="store_true",
                   help="Exit non-zero if no secret is found. Use for release builds.")
    args = p.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    target = args.target or (repo_root / DEFAULT_TARGET)

    if not target.exists():
        print(f"ERROR: target not found: {target}", file=sys.stderr)
        return 2

    contents = target.read_text(encoding="utf-8")
    if PLACEHOLDER not in contents:
        print(f"NOTE: {target.name} already has no {PLACEHOLDER} placeholder — nothing to do.")
        return 0

    secret = resolve_secret(args, repo_root)
    if not secret:
        msg = (f"WARN: no TURN_SECRET found (--secret, env, .env). Leaving {PLACEHOLDER} "
               "in place — TURN fallback will not work in this build.")
        if args.strict:
            print(f"ERROR (strict): {msg}", file=sys.stderr)
            return 1
        print(msg, file=sys.stderr)
        return 0

    if PLACEHOLDER in secret:
        print(f"ERROR: TURN_SECRET value contains the placeholder string itself; refusing.",
              file=sys.stderr)
        return 2

    target.write_text(contents.replace(PLACEHOLDER, secret), encoding="utf-8")
    print(f"OK — substituted TURN_SECRET into {target} ({len(secret)} char credential).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
