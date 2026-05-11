#!/usr/bin/env python3
"""
Prune GitHub Actions artifacts to free up Actions storage.

The 2 GB free Actions storage cap is *account-wide*, not per-repo — so
artifacts from every repo on the account share the same bucket.
Counterintuitively, releasing one repo can saturate the cap for all of
them.

This script enumerates artifacts across one repo (--repo) or every repo
the authenticated `gh` user can list (default), reports total size by
repo, and optionally deletes them. Releases assets live in a separate
storage bucket and are never touched.

Requires the `gh` CLI to be installed and authenticated.

Examples:

  # Dry run — list artifacts across all your repos, total size, no deletes.
  python scripts/prune_actions_artifacts.py

  # Just one repo.
  python scripts/prune_actions_artifacts.py --repo inviti8/infinipaint

  # Restrict enumeration to a specific account when you're a member of many orgs.
  python scripts/prune_actions_artifacts.py --owner inviti8

  # Delete artifacts older than 7 days across all listable repos.
  python scripts/prune_actions_artifacts.py --older-than 7 --delete

  # Nuke everything (use when the account is saturated and you want to start fresh).
  python scripts/prune_actions_artifacts.py --delete
"""

import argparse
import json
import subprocess
import sys
from datetime import datetime, timedelta, timezone


class GhError(RuntimeError):
    pass


def gh(*args: str) -> str:
    """Run `gh` and return stdout, raising GhError on non-zero exit."""
    r = subprocess.run(["gh", *args], capture_output=True, text=True)
    if r.returncode != 0:
        raise GhError(f"gh {' '.join(args)} failed (exit {r.returncode}):\n{r.stderr.strip()}")
    return r.stdout


def list_repos(owner: str | None) -> list[str]:
    """Return `owner/name` strings the authenticated user can list."""
    args = ["repo", "list"]
    if owner:
        args.append(owner)
    args += ["--limit", "1000", "--json", "nameWithOwner"]
    return [r["nameWithOwner"] for r in json.loads(gh(*args))]


def list_artifacts(repo: str) -> list[dict]:
    """All non-expired artifacts in a repo (paginated)."""
    out = gh("api", f"repos/{repo}/actions/artifacts", "--paginate",
             "-q", ".artifacts[]")
    artifacts = []
    for line in out.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            artifacts.append(json.loads(line))
        except json.JSONDecodeError:
            continue  # gh's --paginate occasionally returns a non-JSON header
    return artifacts


def delete_artifact(repo: str, artifact_id: int) -> None:
    gh("api", "-X", "DELETE", f"repos/{repo}/actions/artifacts/{artifact_id}")


def human_size(n: float) -> str:
    for unit in ("B", "KiB", "MiB", "GiB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} TiB"


def parse_iso(s: str) -> datetime:
    return datetime.fromisoformat(s.replace("Z", "+00:00"))


def main() -> None:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("--repo", metavar="OWNER/REPO",
                   help="Only operate on this repo (default: every repo `gh repo list` returns).")
    p.add_argument("--owner", metavar="USER_OR_ORG",
                   help="When enumerating, restrict to repos owned by this user/org.")
    p.add_argument("--older-than", type=int, metavar="DAYS",
                   help="Only consider artifacts older than DAYS days.")
    p.add_argument("--delete", action="store_true",
                   help="Actually delete (default is dry-run print only).")
    args = p.parse_args()

    try:
        repos = [args.repo] if args.repo else list_repos(args.owner)
    except GhError as e:
        sys.exit(str(e))

    cutoff = (datetime.now(timezone.utc) - timedelta(days=args.older_than)
              if args.older_than is not None else None)

    to_delete: list[tuple[str, int, str, int]] = []  # (repo, id, name, size)

    for repo in repos:
        try:
            arts = list_artifacts(repo)
        except GhError as e:
            print(f"  skip {repo}: {e.args[0].splitlines()[0]}", file=sys.stderr)
            continue
        for a in arts:
            if a.get("expired"):
                continue  # already gone from storage
            created = parse_iso(a["created_at"])
            if cutoff and created > cutoff:
                continue
            to_delete.append((repo, a["id"], a["name"], a.get("size_in_bytes", 0)))

    total_size = sum(s for _, _, _, s in to_delete)
    print(f"Found {len(to_delete)} artifacts totaling {human_size(total_size)}")
    if cutoff:
        print(f"(filtered: created before {cutoff.isoformat()})")
    print()

    by_repo: dict[str, list[tuple[int, str, int]]] = {}
    for repo, aid, name, size in to_delete:
        by_repo.setdefault(repo, []).append((aid, name, size))
    for repo, items in sorted(by_repo.items(), key=lambda kv: -sum(s for _, _, s in kv[1])):
        repo_size = sum(s for _, _, s in items)
        print(f"  {repo}: {len(items)} artifacts, {human_size(repo_size)}")

    if not args.delete:
        print()
        print("Dry run. Re-run with --delete to actually prune.")
        return

    if not to_delete:
        print("Nothing to delete.")
        return

    print()
    print(f"Deleting {len(to_delete)} artifacts...")
    deleted = 0
    freed = 0
    for repo, aid, name, size in to_delete:
        try:
            delete_artifact(repo, aid)
            deleted += 1
            freed += size
        except GhError as e:
            print(f"  failed {repo}#{aid} ({name}): {e.args[0].splitlines()[0]}",
                  file=sys.stderr)
    print(f"Deleted {deleted} / {len(to_delete)} artifacts ({human_size(freed)} freed).")


if __name__ == "__main__":
    main()
