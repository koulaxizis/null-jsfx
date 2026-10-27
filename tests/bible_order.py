#!/usr/bin/env python3
"""Checks that the changelog (Part X of NULL_JSFX_BIBLE.md) stays in ascending order (R26).

Every entry heading starts with a date (`### YYYY-MM-DD — ...`), and the dates never go
back: the newest entry is at the bottom. Parallel sessions that put their entry near the
top, or newest-first, fail here in their own PR.

Usage: bible_order.py [path/to/NULL_JSFX_BIBLE.md]   Exit status 1 on any problem.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATE = re.compile(r"^### (\d{4}-\d{2}-\d{2})\b")


def problems(text):
    start = text.find("\n## Part X ")
    if start < 0:
        return ["Part X heading not found"]
    errors, prev, prev_title = [], "", ""
    for line in text[start:].splitlines():
        if not line.startswith("### "):
            continue
        m = DATE.match(line)
        if not m:
            errors.append(f"no YYYY-MM-DD date: {line!r}")
            continue
        if m.group(1) < prev:
            errors.append(f"{line[4:]!r} comes after {prev_title!r}")
        prev, prev_title = m.group(1), line[4:]
    return errors


def main(argv):
    path = Path(argv[0]) if argv else ROOT / "NULL_JSFX_BIBLE.md"
    errs = problems(path.read_text(encoding="utf-8"))
    for e in errs:
        print(f"BIBLE {e}")
    if errs:
        print("Part X is oldest first, newest at the bottom (R26): put a new entry at the very end.")
    print(f"bible_order: {len(errs)} problem(s)")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
