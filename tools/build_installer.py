#!/usr/bin/env python3
"""Builds the NULL JSFX installers from the release packages, so the user picks what to install:
the formats (JSFX, JSFX GFX, VST3, CLAP), the plugins (by group or one by one) and the extras
(FX chains, Rack, tablet remote, Noto Sans).

  Windows        writes components.iss (the plugin tree and its files) next to a copy of
                 installer/windows/null_jsfx.iss; Inno Setup 6 (iscc) then makes
                 NULL-JSFX-Installer-Windows.exe.
  Linux, macOS   writes NULL-JSFX-Installer-<OS>.sh: installer/unix/install.sh with the plugin
                 list filled in and both packages appended as a .tar.gz.

The plugin list (names, groups, which have GFX and VST3/CLAP) comes from the .jsfx headers and
the packages themselves, and every FX chain lists the plugins it uses, so the installers add a
chain only when its plugins are installed too. Nothing here is checked in: CI runs it per release.

Usage: build_installer.py --os Windows|Linux|macOS --jsfx <NULL-JSFX-JSFX dir>
                          --native <NULL-JSFX-VST3-CLAP-<OS> dir> --version <v> --out <dir>
"""
import argparse
import io
import os
import re
import shutil
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BS = "\\"
GROUPS = [("One Slider", "oneslider", "one-slider"), ("Essentials", "essentials", "essentials"),
          ("Advanced", "advanced", "advanced")]
CHAIN_JS = re.compile(r"^<JS null_jsfx/(?:gfx/)?(\w+?)(?:_gfx)?\.jsfx\b", re.M)

sys.dont_write_bytecode = True


def plugins(jsfx, native):
    """[(stem, name, group title, has GFX, has VST3/CLAP)] in name order, from the headers."""
    out = []
    for path in sorted((jsfx / "null_jsfx").glob("*.jsfx")):
        lines = path.read_text(encoding="utf-8").split("\n")
        name = lines[0].split(":", 1)[1].strip()
        name = name[len("NULL "):] if name.startswith("NULL ") else name
        group = lines[3].split(": ", 1)[1].strip()
        if group not in [g[0] for g in GROUPS]:
            sys.exit(f"build_installer: {path.name}: unknown category {group!r}")
        stem = path.stem
        gfx = (jsfx / "null_jsfx/gfx" / f"{stem}_gfx.jsfx").exists()
        nat = (native / "VST3/NULL JSFX" / f"null_jsfx_{stem}.vst3").exists()
        out.append((stem, name, group, gfx, nat))
    if not out:
        sys.exit(f"build_installer: no plugins in {jsfx / 'null_jsfx'}")
    return sorted(out, key=lambda p: p[1].lower())


def chains(jsfx):
    """[(folder, file name, [plugin stems])] for every FX chain in the package."""
    out = []
    for path in sorted((jsfx / "FXChains").glob("*/*.RfxChain")):
        uses = sorted(set(CHAIN_JS.findall(path.read_text(encoding="utf-8", errors="replace"))))
        if not uses:
            sys.exit(f"build_installer: {path}: no NULL JSFX plugins found in it")
        out.append((path.parent.name, path.name, uses))
    return out


def group_key(title, i):
    return next(g[i] for g in GROUPS if g[0] == title)


# --- Windows (Inno Setup) ----------------------------------------------------------------------

def q(s):
    return '"' + str(s).replace('"', '""') + '"'


def windows(args, plugs, chain_list):
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    shutil.copy(ROOT / "installer/windows/null_jsfx.iss", out / "null_jsfx.iss")
    shutil.copy(ROOT / "assets/img/favicon.ico", out / "null_jsfx.ico")
    jsfx, native = args.jsfx.resolve(), args.native.resolve()
    comp, files = ["[Components]"], ["[Files]"]
    every = "full reaper daw"
    for title, key, _ in GROUPS:
        members = [p for p in plugs if p[2] == title]
        comp.append(f"Name: fx\\{key}; Description: {q(f'{title} ({len(members)})')}; Types: {every}")
        for stem, name, _, _, nat in members:
            note = "" if nat else " (JSFX only)"
            comp.append(f"Name: fx\\{key}\\{stem}; Description: {q(name + note)}; Types: {every}")

    def src(path):
        return q(str(path))

    R = "{code:ReaperDir}\\"
    E = R + "Effects\\null_jsfx"
    G = E + "\\gfx"
    for stem, name, group, gfx, nat in plugs:
        c = f"fx\\{group_key(group, 1)}\\{stem}"
        for ext in (".jsfx", ".jsfx.rpl"):
            files.append(f"Source: {src(jsfx / 'null_jsfx' / (stem + ext))}; DestDir: {q(E)}; "
                         f"Components: fmt\\jsfx and {c}; Flags: ignoreversion")
            if gfx:
                files.append(f"Source: {src(jsfx / 'null_jsfx/gfx' / (stem + '_gfx' + ext))}; DestDir: {q(G)}; "
                             f"Components: fmt\\gfx and {c}; Flags: ignoreversion")
        if nat:
            files += native_files(native, f"null_jsfx_{stem}", f"({c})")
    files.append(f"Source: {src(jsfx / 'null_jsfx/gfx/null_gfx.jsfx-inc')}; DestDir: {q(G)}; "
                 "Components: fmt\\gfx; Flags: ignoreversion")
    files += native_files(native, "null_jsfx_rack", "extra\\rack")
    for folder, name, uses in chain_list:
        fmt = "fmt\\gfx" if folder.endswith("GFX") else "fmt\\jsfx"
        need = " and ".join(f"fx\\{group_key(next(p[2] for p in plugs if p[0] == u), 1)}\\{u}" for u in uses)
        files.append(f"Source: {src(jsfx / 'FXChains' / folder / name)}; DestDir: {q(R + 'FXChains' + BS + folder)}; "
                     f"Components: extra\\chains and {fmt} and {need}; Flags: ignoreversion")
    files.append(f"Source: {src(jsfx / 'Remote/reaper_www_root/null_jsfx.html')}; DestDir: {q(R + 'reaper_www_root')}; "
                 "Components: extra\\remote; Flags: ignoreversion")
    files.append(f"Source: {src(jsfx / 'Remote/Scripts/null_jsfx_remote_bridge.lua')}; DestDir: {q(R + 'Scripts')}; "
                 "Components: extra\\remote; Flags: ignoreversion")
    files.append(f"Source: {src(jsfx / 'Remote/INSTALL.txt')}; DestDir: \"{{app}}\"; DestName: Remote-INSTALL.txt; "
                 "Components: extra\\remote; Flags: ignoreversion")
    for font, face in (("NotoSans-Regular.ttf", "Noto Sans"), ("NotoSans-Bold.ttf", "Noto Sans Bold")):
        files.append(f"Source: {src(jsfx / 'fonts' / font)}; DestDir: \"{{autofonts}}\"; FontInstall: {q(face)}; "
                     "Components: extra\\fonts; Flags: onlyifdoesntexist uninsneveruninstall")
    files.append(f"Source: {src(jsfx / 'fonts/OFL.txt')}; DestDir: \"{{app}}\"; DestName: NotoSans-OFL.txt; "
                 "Components: extra\\fonts; Flags: ignoreversion")
    files.append(f"Source: {src(jsfx / 'INSTALL.txt')}; DestDir: \"{{app}}\"; Flags: ignoreversion")
    (out / "components.iss").write_text("\n".join(comp + [""] + files) + "\n", encoding="utf-8-sig")
    print(f"build_installer: {out / 'components.iss'}: {len(plugs)} plugins, {len(chain_list)} FX chains")


def native_files(native, base, when):
    out = []
    for fmt, ext, dest in (("VST3", ".vst3", "{commoncf64}\\VST3\\NULL JSFX"), ("CLAP", ".clap", "{commoncf64}\\CLAP\\NULL JSFX")):
        path = native / fmt / "NULL JSFX" / (base + ext)
        cond = f"Components: fmt\\{fmt.lower()} and {when}"
        if path.is_dir():
            out.append(f"Source: {q(str(path) + BS + '*')}; DestDir: {q(dest + BS + base + ext)}; {cond}; "
                       "Flags: ignoreversion recursesubdirs createallsubdirs")
        elif path.is_file():
            out.append(f"Source: {q(path)}; DestDir: {q(dest)}; {cond}; Flags: ignoreversion")
        else:
            sys.exit(f"build_installer: missing {path}")
    return out


# --- Linux and macOS (self-extracting shell script) --------------------------------------------

def unix(args, plugs, chain_list):
    args.out.mkdir(parents=True, exist_ok=True)
    rows = "\n".join(f"{s}|{n}|{group_key(g, 2)}|{int(gfx)}|{int(nat)}" for s, n, g, gfx, nat in plugs)
    crow = "\n".join(f"{folder}|{name}|{' '.join(uses)}" for folder, name, uses in chain_list)
    for text in (rows, crow):
        if "'" in text:
            sys.exit("build_installer: a plugin or chain name has a ' in it")
    script = (ROOT / "installer/unix/install.sh").read_text(encoding="utf-8")
    for key, value in (("@@PLUGINS@@", rows), ("@@CHAINS@@", crow), ("@@VERSION@@", args.version), ("@@OS@@", args.os)):
        script = script.replace(key, value)
    if not script.endswith("\n__PAYLOAD_BELOW__\n"):
        sys.exit("build_installer: install.sh must end with the __PAYLOAD_BELOW__ line")
    payload = io.BytesIO()
    with tarfile.open(fileobj=payload, mode="w:gz", compresslevel=9) as tar:
        tar.add(args.jsfx, arcname="jsfx", filter=clean)
        tar.add(args.native, arcname="native", filter=clean)
    path = args.out / f"NULL-JSFX-Installer-{args.os}.sh"
    path.write_bytes(script.encode("utf-8") + payload.getvalue())
    os.chmod(path, 0o755)
    print(f"build_installer: {path}: {len(plugs)} plugins, {len(chain_list)} FX chains, {path.stat().st_size // 1024} KB")


def clean(info):
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    return info


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--os", required=True, choices=["Windows", "Linux", "macOS"])
    ap.add_argument("--jsfx", required=True, type=Path)
    ap.add_argument("--native", required=True, type=Path)
    ap.add_argument("--version", default="dev")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    plugs, chain_list = plugins(args.jsfx, args.native), chains(args.jsfx)
    stems = {p[0] for p in plugs}
    for folder, name, uses in chain_list:
        missing = set(uses) - stems
        if missing:
            sys.exit(f"build_installer: {folder}/{name} uses unknown plugins: {' '.join(sorted(missing))}")
    (windows if args.os == "Windows" else unix)(args, plugs, chain_list)


if __name__ == "__main__":
    main()
