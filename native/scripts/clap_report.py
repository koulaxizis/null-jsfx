#!/usr/bin/env python3
"""Group the failed tests of clap-validator's JSON reports (scripts/validate.sh clap).

    clap_report.py <reports>/clap-validator

Prints a Markdown table: one row per test and message (its first and last line, numbers
masked as #), with the number of plugins and their names, the largest groups first.
"""
import collections
import glob
import json
import os
import re
import sys

groups = collections.defaultdict(list)
for f in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
    name = os.path.basename(f)[: -len(".json")].removeprefix("null_jsfx_")
    try:
        with open(f, encoding="utf-8") as fp:
            results = json.load(fp)["results"]
    except (OSError, ValueError, KeyError):
        groups[("(no JSON report)", "")].append(name)
        continue
    for r in results:
        if r["status"]["code"] != "failed":
            continue
        test = next(iter(r["test"].values()))["test"]
        lines = (r["status"].get("details") or "").strip().splitlines()
        msg = " ... ".join(lines[:1] + lines[-1:] if len(lines) > 1 else lines)
        msg = re.sub(r"\d+(\.\d+)?", "#", msg)[:200]
        groups[(test, msg)].append(name)

print("| Plugins | Test | Message | Which |")
print("|---|---|---|---|")
for (test, msg), names in sorted(groups.items(), key=lambda g: -len(g[1])):
    print(f"| {len(names)} | `{test}` | {msg.replace('|', '/')} | {' '.join(names)} |")
