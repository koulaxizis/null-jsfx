#!/usr/bin/env python3
"""Writes NULL-JSFX-Catalog.json: what NULL JSFX Hub (hub/) shows and installs for a release.

Every release carries it next to the packages, so the Hub reads it from
https://github.com/koulaxizis/null-jsfx/releases/latest/download/NULL-JSFX-Catalog.json and
the catalog always matches the zips it installs from. It holds the plugins (names, groups, text,
sliders, formats, screenshot, factory preset names), the FX chains with the plugins each one uses,
the extras, the package names and the release notes. Names and formats come from the .jsfx
headers through tools/build_site.py; the text from tools/site_content.json and
tools/site_chains.json.

Usage: build_catalog.py --version 2.2 [--notes FILE] [--date YYYY-MM-DD] --out FILE
       build_catalog.py --check      build it in memory and check it (used by CI)
"""
import argparse
import datetime
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools"))
from build_site import CATEGORIES, load  # noqa: E402

SITE = "https://nulljsfx.tech/"
REPO = "koulaxizis/null-jsfx"
CHAINS_DIR = ROOT / "DATA/FXChains"
CHAIN_TEXT = ROOT / "tools/site_chains.json"
PRESETS = ROOT / "tools/presets"
CHAIN_JS = re.compile(r"^<JS null_jsfx/(?:gfx/)?(\w+?)(?:_gfx)?\.jsfx\b", re.M)
SCHEMA = 1
OSES = ("Windows", "Linux", "macOS")

# the Hub's own download per OS (made by .github/workflows/hub.yml)
HUB_ASSETS = {
    "Windows": "NULL-JSFX-Hub-Windows-Setup.exe",
    "Linux": "NULL-JSFX-Hub-Linux.AppImage",
    "macOS": "NULL-JSFX-Hub-macOS.dmg",
}

EXTRAS = [
    {"id": "chains", "name": "FX chains", "formats": ["jsfx", "gfx"],
     "desc": "Ready-made REAPER FX chains of NULL JSFX plugins. A chain is added when all its plugins are installed."},
    {"id": "presets", "name": "Factory presets", "formats": ["jsfx", "gfx"],
     "desc": "The factory presets in REAPER's preset menu of each JSFX, added next to your own presets."},
    {"id": "rack", "name": "Rack", "formats": ["vst3", "clap"],
     "desc": "Up to 8 NULL JSFX effects in one VST3/CLAP, with LFOs, an envelope, macros and 52 presets."},
    {"id": "remote", "name": "Tablet remote", "formats": ["jsfx", "gfx"],
     "desc": "Control REAPER's mixer and the NULL JSFX plugins from a tablet. Needs a few steps in REAPER after."},
    {"id": "fonts", "name": "Noto Sans font", "formats": ["gfx"],
     "desc": "The font of the GFX versions, for their exact look."},
]


def plugins():
    out = []
    for p in load():
        stem = p["stem"]
        presets = PRESETS / f"{stem}.json"
        fmts = ["jsfx"] + [f.lower() for f in p["formats"][1:]]
        out.append({
            "id": stem,
            "name": p["name"],
            "group": next(key for title, key, _ in CATEGORIES if title == p["category"]),
            "tagline": p["card_desc"],
            "keywords": p["keywords"],
            "uses": p["uses"],
            "properties": p["properties"],
            "sliders": [{"name": n, "range": r, "desc": d} for n, r, d in p["sliders"]],
            "formats": fmts,
            "shot": SITE + p["shot"] if p["shot"] else None,
            "guide": f"{SITE}#{p['key']}",
            "presets": [name for name, _ in json.loads(presets.read_text(encoding="utf-8"))] if presets.exists() else [],
        })
    return out


def chains(stems):
    text = json.loads(CHAIN_TEXT.read_text(encoding="utf-8"))
    by_name = {}
    for path in sorted(CHAINS_DIR.glob("*/*.RfxChain")):
        uses = sorted(set(CHAIN_JS.findall(path.read_text(encoding="utf-8", errors="replace"))))
        unknown = set(uses) - stems
        if not uses or unknown:
            sys.exit(f"build_catalog: {path}: plugins not in the catalog: {' '.join(sorted(unknown)) or 'none found'}")
        c = by_name.setdefault(path.stem, {"name": path.stem, "file": path.name, "desc": text.get(path.stem, ""),
                                           "plugins": uses, "folders": {}})
        c["folders"]["gfx" if path.parent.name.endswith("GFX") else "jsfx"] = path.parent.name
        if c["plugins"] != uses:
            sys.exit(f"build_catalog: {path.stem}: the JSFX and GFX chains use different plugins")
    return sorted(by_name.values(), key=lambda c: c["name"].lower())


def catalog(version, notes, date):
    plugs = plugins()
    return {
        "schema": SCHEMA,
        "version": version,
        "date": date,
        "notes": notes,
        "repo": REPO,
        "site": SITE,
        "packages": {"jsfx": "NULL-JSFX-JSFX.zip",
                     "native": {os: f"NULL-JSFX-VST3-CLAP-{os}.zip" for os in OSES}},
        "hub": {"version": version, "assets": HUB_ASSETS},
        "groups": [{"id": key, "name": title, "desc": desc} for title, key, desc in CATEGORIES],
        "plugins": plugs,
        "chains": chains({p["id"] for p in plugs}),
        "extras": EXTRAS,
    }


def check(cat):
    errors = []
    ids = [p["id"] for p in cat["plugins"]]
    if len(ids) != len(set(ids)):
        errors.append("duplicate plugin ids")
    for p in cat["plugins"]:
        if not re.fullmatch(r"[a-z0-9_]+", p["id"]):
            errors.append(f"{p['id']}: id must be [a-z0-9_]")
        if not p["formats"] or p["formats"][0] != "jsfx":
            errors.append(f"{p['id']}: formats must start with jsfx")
    if not cat["chains"]:
        errors.append("no FX chains")
    if errors:
        sys.exit("build_catalog: " + "; ".join(errors))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--version", default="dev")
    ap.add_argument("--notes", type=Path, help="release notes (markdown)")
    ap.add_argument("--date", default=datetime.date.today().isoformat())
    ap.add_argument("--out", type=Path)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    notes = args.notes.read_text(encoding="utf-8") if args.notes and args.notes.exists() else ""
    cat = catalog(args.version, notes, args.date)
    check(cat)
    if args.check:
        print(f"build_catalog: ok, {len(cat['plugins'])} plugins, {len(cat['chains'])} FX chains")
        return
    if not args.out:
        sys.exit("build_catalog: --out or --check")
    args.out.write_text(json.dumps(cat, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"build_catalog: {args.out}: {len(cat['plugins'])} plugins, {len(cat['chains'])} FX chains")


if __name__ == "__main__":
    main()
