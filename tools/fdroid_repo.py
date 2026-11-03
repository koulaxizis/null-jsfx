#!/usr/bin/env python3
"""Helpers for our own F-Droid repository of the Android remotes (Bible II.10).

The workflow .github/workflows/fdroid-repo.yml builds the "fdroid" flavor of each app in
APPS, signed with the same key as the APKs on our GitHub releases, and publishes them with
`fdroid update` to GitHub Pages. F-Droid and Droid-ify add the repository; Obtainium reads it
too. This script holds the parts that can be tested without the Android SDK:

  meta <dir>               write <dir>/metadata/<package>.yml and the store texts from the
                           fastlane folders of each app (android/app/src/<app>/fastlane)
  name <apk> <package> <versionCode>
                           print the file name the APK gets in the repository:
                           <package>_<versionCode>_<sha256 first 8>.apk (a new name for every
                           build, so a CDN or a half-finished download never mixes two builds)
  prune <repo dir> [keep]  keep the newest `keep` versions (default 3) of each package, so a
                           phone holding an older index still finds its file
"""
import hashlib
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# app flavor -> application id. Released identities never change (Bible R3).
APPS = {
    "daws": "tech.nulljsfx.remote.daws",
    "mixers": "tech.nulljsfx.remote.mixers",
}

COMMON = """License: MIT
AuthorName: Christos Koulaxizis
WebSite: https://nulljsfx.tech
SourceCode: https://github.com/koulaxizis/null-jsfx
IssueTracker: https://github.com/koulaxizis/null-jsfx/issues
Donate: https://ko-fi.com/koulaxizis
Categories:
  - Multimedia
"""

APK_NAME = re.compile(r"^([A-Za-z0-9_.]+)_([0-9]+)_[0-9a-f]{8}\.apk$")


def fastlane_dir(app):
    return os.path.join(ROOT, "android", "app", "src", app, "fastlane", "metadata", "android")


def write_meta(out):
    """metadata/<pkg>.yml plus metadata/<pkg>/<locale>/ (title, short and full description,
    icon of each app), the layout `fdroid update` reads for a repository of ready APKs."""
    meta = os.path.join(out, "metadata")
    os.makedirs(meta, exist_ok=True)
    for app, pkg in APPS.items():
        src = fastlane_dir(app)
        # each app has its own icon (fastlane's images/icon.png, rendered from
        # android/fdroid/icon-<app>.svg); the repository keeps the NULL JSFX one
        icon = os.path.join(src, "en-US", "images", "icon.png")
        if not os.path.isfile(icon):
            raise SystemExit(f"{app}: no icon in {icon}")
        locales = sorted(d for d in os.listdir(src) if os.path.isdir(os.path.join(src, d)))
        if "en-US" not in locales:
            raise SystemExit(f"{app}: no en-US store texts in {src}")
        for loc in locales:
            dst = os.path.join(meta, pkg, loc)
            os.makedirs(os.path.join(dst, "images"), exist_ok=True)
            for f in ("title.txt", "short_description.txt", "full_description.txt"):
                p = os.path.join(src, loc, f)
                if not os.path.isfile(p):
                    raise SystemExit(f"{app}/{loc}: missing {f}")
                text = open(p, encoding="utf-8").read().strip()
                if f == "short_description.txt" and len(text) > 80:
                    raise SystemExit(f"{app}/{loc}: short description has {len(text)} characters (F-Droid allows 80)")
                if f == "title.txt" and len(text) > 50:
                    raise SystemExit(f"{app}/{loc}: title has {len(text)} characters (F-Droid allows 50)")
                with open(os.path.join(dst, f), "w", encoding="utf-8") as o:
                    o.write(text + "\n")
            own = os.path.join(src, loc, "images", "icon.png")
            shutil.copyfile(own if os.path.isfile(own) else icon, os.path.join(dst, "images", "icon.png"))
        with open(os.path.join(meta, pkg + ".yml"), "w", encoding="utf-8") as o:
            o.write(COMMON)
    return sorted(APPS.values())


def repo_name(apk, pkg, code):
    if pkg not in APPS.values():
        raise SystemExit(f"{pkg} is not one of our apps")
    if not re.fullmatch(r"[0-9]+", str(code)):
        raise SystemExit(f"bad versionCode {code!r}")
    h = hashlib.sha256(open(apk, "rb").read()).hexdigest()[:8]
    return f"{pkg}_{code}_{h}.apk"


def prune(repo, keep=3):
    """Removes all but the newest `keep` versionCodes of each package; returns the removed
    file names. Files that are not ours (other names) stay."""
    by_pkg = {}
    for f in os.listdir(repo):
        m = APK_NAME.match(f)
        if m:
            by_pkg.setdefault(m.group(1), []).append((int(m.group(2)), f))
    removed = []
    for pkg, files in by_pkg.items():
        codes = sorted({c for c, _ in files}, reverse=True)[:keep]
        for c, f in files:
            if c not in codes:
                os.remove(os.path.join(repo, f))
                removed.append(f)
    return sorted(removed)


def main(argv):
    if len(argv) >= 2 and argv[0] == "meta":
        print("\n".join(write_meta(argv[1])))
    elif len(argv) == 4 and argv[0] == "name":
        print(repo_name(argv[1], argv[2], argv[3]))
    elif len(argv) in (2, 3) and argv[0] == "prune":
        for f in prune(argv[1], int(argv[2]) if len(argv) == 3 else 3):
            print("removed", f)
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
