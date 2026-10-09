#!/usr/bin/env python3
"""Writes a ReaPack index (index.xml) for NULL JSFX.

Every plain JSFX becomes one package in its category (One Slider, Essentials, Advanced), so
REAPER users can install single plugins and get updates. The GFX versions and their shared
null_gfx.jsfx-inc go into one "GFX" package, because they need that file next to them and
ReaPack has no dependencies between packages.

The file URLs point at a fixed git ref (the release tag), so an index always installs the
files it was made for. CI attaches it to each published release; users add
    https://github.com/koulaxizis/null-jsfx/releases/latest/download/index.xml
in REAPER: Extensions > ReaPack > Import repositories.

Usage: tools/build_reapack.py --ref v2.0 [--version 2.0] [--out index.xml]
"""
import argparse
import datetime
import pathlib
import re
import sys
from xml.sax.saxutils import escape, quoteattr

ROOT = pathlib.Path(__file__).resolve().parent.parent
FX = ROOT / "DATA" / "Effects" / "null_jsfx"
REPO = "koulaxizis/null-jsfx"
AUTHOR = "Christos Koulaxizis"
CATEGORIES = ["One Slider", "Essentials", "Advanced"]


def header(path):
    desc = category = None
    for line in path.read_text(encoding="utf-8").splitlines()[:8]:
        if line.startswith("desc:"):
            desc = line[5:].strip()
        m = re.match(r"//\s*category:\s*(.+)", line)
        if m:
            category = m.group(1).strip()
    return desc, category


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", required=True, help="git tag or commit the file URLs point at")
    ap.add_argument("--version", help="package version (default: --ref without a leading v)")
    ap.add_argument("--out", default="index.xml")
    args = ap.parse_args()
    version = args.version or re.sub(r"^v", "", args.ref)
    if not re.fullmatch(r"\d+(\.\d+)*", version):
        sys.exit(f"build_reapack: version '{version}' must be numbers separated by dots")
    when = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    def url(path):
        return f"https://raw.githubusercontent.com/{REPO}/{args.ref}/{path.relative_to(ROOT).as_posix()}"

    def ver(sources):
        lines = [f'      <version name="{version}" author={quoteattr(AUTHOR)} time="{when}">']
        lines += [f"        <source{attrs}>{escape(u)}</source>" for attrs, u in sources]
        lines.append("      </version>")
        return lines

    by_cat = {c: [] for c in CATEGORIES}
    for path in sorted(FX.glob("*.jsfx")):
        desc, category = header(path)
        if category not in by_cat:
            sys.exit(f"build_reapack: {path.name} has no known category")
        by_cat[category].append((path, desc))

    out = ['<?xml version="1.0" encoding="utf-8"?>', '<index version="1" name="NULL JSFX">',
           "  <metadata>", '    <link rel="website">https://nulljsfx.tech</link>',
           "  </metadata>"]
    for category in CATEGORIES:
        out.append(f"  <category name={quoteattr(category)}>")
        for path, desc in by_cat[category]:
            out.append(f'    <reapack name={quoteattr(path.name)} type="effect" desc={quoteattr(desc)}>')
            out += ver([("", url(path))])
            out.append("    </reapack>")
        out.append("  </category>")

    gfx = sorted((FX / "gfx").glob("*.jsfx")) + sorted((FX / "gfx").glob("*.jsfx-inc"))
    out.append('  <category name="GFX">')
    out.append('    <reapack name="NULL JSFX GFX" type="effect" '
               'desc="NULL JSFX GFX versions (own interface)">')
    out += ver([(f" file={quoteattr(p.name)}", url(p)) for p in gfx])
    out.append("    </reapack>")
    out.append("  </category>")
    out.append("</index>")

    pathlib.Path(args.out).write_text("\n".join(out) + "\n", encoding="utf-8")
    n = sum(len(v) for v in by_cat.values())
    print(f"build_reapack: {n} plugins + {len(gfx)} GFX files, version {version}, ref {args.ref} -> {args.out}")


if __name__ == "__main__":
    main()
