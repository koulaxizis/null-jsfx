#!/usr/bin/env python3
"""Tests tools/fdroid_repo.py: the store texts F-Droid gets, the APK names and the pruning."""
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
import fdroid_repo  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


with tempfile.TemporaryDirectory() as d:
    pkgs = fdroid_repo.write_meta(d)
    check(pkgs == ["tech.nulljsfx.remote.daws", "tech.nulljsfx.remote.mixers"], "both apps, ids unchanged (R3)")
    for p in pkgs:
        yml = open(os.path.join(d, "metadata", p + ".yml"), encoding="utf-8").read()
        check("License: MIT" in yml and "https://ko-fi.com/koulaxizis" in yml, p + ": MIT and Ko-fi")
        loc = os.path.join(d, "metadata", p, "en-US")
        for f in ("title.txt", "short_description.txt", "full_description.txt", "images/icon.png"):
            check(os.path.getsize(os.path.join(loc, f)) > 0, p + ": " + f)
        short = open(os.path.join(loc, "short_description.txt"), encoding="utf-8").read().strip()
        check(len(short) <= 80, p + ": summary %d <= 80 characters" % len(short))
    mixers = open(os.path.join(d, "metadata", "tech.nulljsfx.remote.mixers", "en-US", "short_description.txt")).read()
    check(mixers.lower().startswith("beta"), "Remote for Mixers says beta")

with tempfile.TemporaryDirectory() as d:
    apk = os.path.join(d, "a.apk")
    open(apk, "wb").write(b"apk bytes")
    n = fdroid_repo.repo_name(apk, "tech.nulljsfx.remote.daws", "30100")
    check(fdroid_repo.APK_NAME.match(n) is not None and n.startswith("tech.nulljsfx.remote.daws_30100_"), "name " + n)
    open(apk, "wb").write(b"other bytes")
    check(fdroid_repo.repo_name(apk, "tech.nulljsfx.remote.daws", "30100") != n, "a new build gets a new name")
    try:
        fdroid_repo.repo_name(apk, "com.example", "1")
        check(False, "a foreign package is refused")
    except SystemExit:
        check(True, "a foreign package is refused")

with tempfile.TemporaryDirectory() as d:
    for pkg, codes in (("tech.nulljsfx.remote.daws", (30000, 30100, 30200, 30300)), ("tech.nulljsfx.remote.mixers", (30300,))):
        for c in codes:
            open(os.path.join(d, "%s_%d_0123abcd.apk" % (pkg, c)), "w").close()
    open(os.path.join(d, "index-v2.json"), "w").close()
    removed = fdroid_repo.prune(d, 3)
    check(removed == ["tech.nulljsfx.remote.daws_30000_0123abcd.apk"], "prune keeps the newest 3 of each app")
    check(len(os.listdir(d)) == 5, "other files and the other app stay")

print("%d failure(s)" % fails)
sys.exit(1 if fails else 0)
