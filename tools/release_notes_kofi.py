#!/usr/bin/env python3
"""Replaces the old Ko-fi lines in the bodies of published GitHub releases (Christos, 2026-11-21).

Ko-fi is gone (R21, R22). Older release notes still ended with a Ko-fi support or "follow on Ko-fi"
line; this rewrites only those lines, to the /support/ page and the Updates feed on koulaxizis.gr.
Assets, tags, names and every other word stay as they are. History lines that say Ko-fi is gone
(4.1.1) are left alone. Idempotent: a release without an old line is not touched.

Usage: release_notes_kofi.py [--dry-run]   (needs GH_TOKEN and GITHUB_REPOSITORY; uses the gh CLI)
"""
import json, os, subprocess, sys

SUPPORT = "https://nulljsfx.tech/support/"
UPDATES = "https://koulaxizis.gr/#updates"
REPLACEMENTS = [
    ("Free forever. Support: https://ko-fi.com/koulaxizis", "Free forever. Support: " + SUPPORT),
    ("Support on Ko-fi: https://ko-fi.com/koulaxizis", "Support: " + SUPPORT),
    ("Support and news: https://ko-fi.com/koulaxizis", "Support: " + SUPPORT + " · News: " + UPDATES),
    ("Follow NULL JSFX on Ko-fi to hear about new versions: https://ko-fi.com/koulaxizis",
     "New versions are announced in the Updates feed on koulaxizis.gr: " + UPDATES),
    ("follow NULL JSFX on Ko-fi (https://ko-fi.com/koulaxizis) to hear about new versions",
     "new versions are announced in the Updates feed on koulaxizis.gr (" + UPDATES + ")"),
]


def gh(*args, data=None):
    out = subprocess.run(["gh", "api", *args], input=data, capture_output=True, text=True, check=True)
    return out.stdout


def main():
    dry = "--dry-run" in sys.argv
    repo = os.environ["GITHUB_REPOSITORY"]
    releases = json.loads(gh("--paginate", "--slurp", f"repos/{repo}/releases?per_page=100"))
    releases = [r for page in releases for r in page]
    changed = 0
    for r in releases:
        body = r.get("body") or ""
        new = body
        for old, rep in REPLACEMENTS:
            new = new.replace(old, rep)
        if new == body:
            continue
        changed += 1
        left = [l for l in new.splitlines() if "ko-fi.com" in l.lower()]
        print(f"{r['tag_name']}: rewritten" + (f" (still names ko-fi.com: {left})" if left else ""))
        if not dry:
            gh("-X", "PATCH", f"repos/{repo}/releases/{r['id']}", "--input", "-",
               data=json.dumps({"body": new}))
    print(f"{changed} release(s) {'would be ' if dry else ''}rewritten")


if __name__ == "__main__":
    main()
