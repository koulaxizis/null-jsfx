#!/usr/bin/env python3
"""Builds the NULL JSFX installers from the release packages, so the user picks what to install:
the formats (JSFX, JSFX GFX, VST3, CLAP, and LV2 on Linux), the plugins (by group or one by one) and the extras
(FX chains, Rack, tablet remote, Noto Sans).

  Windows        writes components.iss (the plugin tree and its files) next to a copy of
                 installer/windows/null_jsfx.iss; Inno Setup 6 (iscc) then makes
                 NULL-JSFX-Installer-Windows.exe.
  Linux, macOS   writes NULL-JSFX-Installer-<OS>.sh: installer/unix/install.sh with the plugin
                 list filled in and both packages appended as a .tar.gz.
  macOS          also writes NULL-JSFX-Installer-macOS.pkg (pkgbuild and productbuild, so only on
                 a Mac): one part per format and group plus the extras, picked under Customize,
                 installed into the user's home (no password); installer/macos/postinstall adds
                 the presets and FX chains, installer/macos/uninstall.command removes it all.
                 --pkg-root DIR stops before pkgbuild and leaves the parts in DIR (for tests).

The plugin list (names, groups, which have GFX and VST3/CLAP) comes from the .jsfx headers and
the packages themselves, and every FX chain lists the plugins it uses, so the installers add a
chain only when its plugins are installed too. The JSFX presets come as REAPER user preset files
(presets/js-*.ini in the JSFX package, from tools/build_presets.py --ini); both installers add
them to the user's own preset files instead of replacing them. Nothing here is checked in: CI runs it per release.

Usage: build_installer.py --os Windows|Linux|macOS --jsfx <NULL-JSFX-JSFX dir>
                          --native <NULL-JSFX-VST3-CLAP-<OS> dir> --version <v> --out <dir>
"""
import argparse
import html
import io
import os
import plistlib
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BS = "\\"
GROUPS = [("One Slider", "oneslider", "one-slider"), ("Essentials", "essentials", "essentials"),
          ("Advanced", "advanced", "advanced"), ("Assisted", "assisted", "assisted"), ("Instruments", "instruments", "instruments"),
          ("MIDI", "midi", "midi")]
CHAIN_JS = re.compile(r"^<JS null_jsfx/(?:gfx/)?(\w+?)(?:_gfx)?\.jsfx\b", re.M)

sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools"))
from build_presets import ini_name  # noqa: E402


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


def arch_suffix(args):
    """-ARM64 for the ARM packages and installers, nothing for x64 and macOS (universal)."""
    return "-ARM64" if args.arch == "arm64" else ""


def windows(args, plugs, chain_list):
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    # the CPU goes in as a define, so a plain "iscc null_jsfx.iss" builds the right installer
    iss = (ROOT / "installer/windows/null_jsfx.iss").read_text(encoding="utf-8")
    (out / "null_jsfx.iss").write_text(f'#define Arch "{args.arch}"\n' + iss, encoding="utf-8")
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
        files.append(f"Source: {src(jsfx / 'null_jsfx' / (stem + '.jsfx'))}; DestDir: {q(E)}; "
                     f"Components: fmt\\jsfx and {c}; Flags: ignoreversion")
        files += preset_file(jsfx, f"null_jsfx/{stem}.jsfx", f"fmt\\jsfx and {c}")
        if gfx:
            files.append(f"Source: {src(jsfx / 'null_jsfx/gfx' / (stem + '_gfx.jsfx'))}; DestDir: {q(G)}; "
                         f"Components: fmt\\gfx and {c}; Flags: ignoreversion")
            files += preset_file(jsfx, f"null_jsfx/gfx/{stem}_gfx.jsfx", f"fmt\\gfx and {c}")
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


def preset_file(jsfx, effect, when):
    """The plugin's REAPER user preset file goes to {app}\\presets; the installer's code then adds
    its presets to the one in REAPER's presets folder (see MergePresets in null_jsfx.iss)."""
    path = jsfx / "presets" / ini_name(effect)
    if not path.is_file():
        sys.exit(f"build_installer: missing {path}")
    return [f"Source: {q(path)}; DestDir: \"{{app}}\\presets\"; Components: {when}; Flags: ignoreversion"]


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
    for key, value in (("@@PLUGINS@@", rows), ("@@CHAINS@@", crow), ("@@VERSION@@", args.version), ("@@OS@@", args.os),
                       ("@@ARCH@@", {"x64": "x86_64", "arm64": "aarch64", "universal": ""}[args.arch]),
                       ("@@SUFFIX@@", arch_suffix(args))):
        script = script.replace(key, value)
    if not script.endswith("\n__PAYLOAD_BELOW__\n"):
        sys.exit("build_installer: install.sh must end with the __PAYLOAD_BELOW__ line")
    payload = io.BytesIO()
    with tarfile.open(fileobj=payload, mode="w:gz", compresslevel=9) as tar:
        tar.add(args.jsfx, arcname="jsfx", filter=clean)
        tar.add(args.native, arcname="native", filter=clean)
    path = args.out / f"NULL-JSFX-Installer-{args.os}{arch_suffix(args)}.sh"
    path.write_bytes(script.encode("utf-8") + payload.getvalue())
    os.chmod(path, 0o755)
    print(f"build_installer: {path}: {len(plugs)} plugins, {len(chain_list)} FX chains, {path.stat().st_size // 1024} KB")


def clean(info):
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    return info


# --- macOS (.pkg) ------------------------------------------------------------------------------

PKG_ID = "tech.nulljsfx.pkg"
REAPER_REL = "Library/Application Support/REAPER"
DATA_REL = "Library/Application Support/NULL JSFX"
FORMAT_TITLES = [("jsfx", "JSFX for REAPER"), ("gfx", "JSFX with the NULL look (GFX)"), ("vst3", "VST3"), ("clap", "CLAP")]


class Part:
    """One component package: its files under a root that stands for the user's home."""

    def __init__(self, base, key, title, about, on=True):
        self.key, self.title, self.about, self.on = key, title, about, on
        self.root = base / "roots" / key
        self.files = []

    def put(self, src, rel):
        dest = self.root / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        if src.is_dir():
            shutil.copytree(src, dest, dirs_exist_ok=True)
        else:
            shutil.copy2(src, dest)
        self.files.append(rel)

    def stage(self, src, rel):
        """For the postinstall script: copied to the staging folder, not noted for removal."""
        dest = self.root / DATA_REL / "staging" / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)

    def finish(self):
        receipt = self.root / DATA_REL / "receipts" / f"{self.key}.txt"
        receipt.parent.mkdir(parents=True, exist_ok=True)
        receipt.write_text("".join(f"{f}\n" for f in sorted(set(self.files))), encoding="utf-8")


def pkg_parts(args, plugs, chain_list, base):
    """[(choice group title or None, [Part])] in install order (the FX chains last)."""
    jsfx, native = args.jsfx.resolve(), args.native.resolve()
    tree = []
    for fmt, ftitle in FORMAT_TITLES:
        parts = []
        for gtitle, _, gkey in GROUPS:
            members = [p for p in plugs if p[2] == gtitle and (fmt in ("jsfx", "gfx") or p[4]) and (fmt != "gfx" or p[3])]
            if not members:
                continue
            part = Part(base, f"{fmt}.{gkey}", f"{gtitle} ({len(members)})",
                        f"{ftitle}: " + ", ".join(p[1] for p in members))
            for stem, *_ in members:
                if fmt == "jsfx":
                    part.put(jsfx / "null_jsfx" / f"{stem}.jsfx", f"{REAPER_REL}/Effects/null_jsfx/{stem}.jsfx")
                    effect = f"null_jsfx/{stem}.jsfx"
                elif fmt == "gfx":
                    part.put(jsfx / "null_jsfx/gfx" / f"{stem}_gfx.jsfx", f"{REAPER_REL}/Effects/null_jsfx/gfx/{stem}_gfx.jsfx")
                    effect = f"null_jsfx/gfx/{stem}_gfx.jsfx"
                else:
                    name = f"null_jsfx_{stem}.{fmt}"
                    part.put(native / fmt.upper() / "NULL JSFX" / name, f"Library/Audio/Plug-Ins/{fmt.upper()}/NULL JSFX/{name}")
                    continue
                ini = jsfx / "presets" / ini_name(effect)
                if not ini.is_file():
                    sys.exit(f"build_installer: missing {ini}")
                part.stage(ini, f"presets/{ini.name}")
            if fmt == "gfx":
                part.put(jsfx / "null_jsfx/gfx/null_gfx.jsfx-inc", f"{REAPER_REL}/Effects/null_jsfx/gfx/null_gfx.jsfx-inc")
            parts.append(part)
        if fmt in ("vst3", "clap"):
            part = Part(base, f"{fmt}.rack", "Rack", "Several NULL JSFX plugins in one, in any order")
            name = f"null_jsfx_rack.{fmt}"
            part.put(native / fmt.upper() / "NULL JSFX" / name, f"Library/Audio/Plug-Ins/{fmt.upper()}/NULL JSFX/{name}")
            parts.append(part)
        tree.append((ftitle, parts))
    extras = []
    part = Part(base, "remote", "Tablet remote for REAPER", "Control the NULL JSFX plugins from a tablet or phone", on=False)
    part.put(jsfx / "Remote/reaper_www_root/null_jsfx.html", f"{REAPER_REL}/reaper_www_root/null_jsfx.html")
    part.put(jsfx / "Remote/Scripts/null_jsfx_remote_bridge.lua", f"{REAPER_REL}/Scripts/null_jsfx_remote_bridge.lua")
    part.put(jsfx / "Remote/INSTALL.txt", f"{DATA_REL}/Remote-INSTALL.txt")
    extras.append(part)
    part = Part(base, "fonts", "Noto Sans", "The font of the NULL look, for the GFX versions")
    for font in ("NotoSans-Regular.ttf", "NotoSans-Bold.ttf"):
        part.put(jsfx / "fonts" / font, f"Library/Fonts/{font}")
    part.put(jsfx / "fonts/OFL.txt", f"{DATA_REL}/NotoSans-OFL.txt")
    extras.append(part)
    # the chains go in last: postinstall adds only those whose plugins are installed by then
    part = Part(base, "chains", "REAPER FX chains", "Ready chains of NULL JSFX plugins, for the JSFX versions you install")
    for folder, name, _ in chain_list:
        part.stage(jsfx / "FXChains" / folder / name, f"chains/{folder}/{name}")
    lines = "".join(f"{folder}|{name}|{' '.join(uses)}\n" for folder, name, uses in chain_list)
    (part.root / DATA_REL / "staging/chains/chains.txt").write_text(lines, encoding="utf-8")
    extras.append(part)
    tree.append(("Extras", extras))
    core = Part(base, "base", "NULL JSFX", "")
    core.put(jsfx / "INSTALL.txt", f"{DATA_REL}/INSTALL.txt")
    core.put(ROOT / "installer/macos/uninstall.command", f"{DATA_REL}/Uninstall NULL JSFX.command")
    os.chmod(core.root / DATA_REL / "Uninstall NULL JSFX.command", 0o755)
    tree.insert(0, (None, [core]))
    for _, parts in tree:
        for part in parts:
            part.finish()
    return tree


def distribution(tree, version):
    e = lambda s: html.escape(s, quote=True)  # noqa: E731
    gid = lambda title: "group." + re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")  # noqa: E731
    parts = [p for _, ps in tree for p in ps]
    out = ['<?xml version="1.0" encoding="utf-8"?>', '<installer-gui-script minSpecVersion="2">',
           f"  <title>NULL JSFX {e(version)}</title>",
           '  <welcome file="welcome.html" mime-type="text/html"/>',
           '  <license file="LICENSE.txt"/>',
           '  <conclusion file="conclusion.html" mime-type="text/html"/>',
           '  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>',
           '  <domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="false"/>',
           "  <choices-outline>"]
    for title, ps in tree:
        if title is None:
            out += [f'    <line choice="{p.key}"/>' for p in ps]
            continue
        out.append(f'    <line choice="{gid(title)}">')
        out += [f'      <line choice="{p.key}"/>' for p in ps]
        out.append("    </line>")
    out.append("  </choices-outline>")
    for title, ps in tree:
        if title is not None:
            out.append(f'  <choice id="{gid(title)}" title="{e(title)}"/>')
        for p in ps:
            hidden = ' visible="false" enabled="false"' if title is None else ""
            off = "" if p.on else ' start_selected="false"'
            out.append(f'  <choice id="{p.key}" title="{e(p.title)}" description="{e(p.about)}"{hidden}{off}>')
            out.append(f'    <pkg-ref id="{PKG_ID}.{p.key}"/>')
            out.append("  </choice>")
    for p in parts:
        out.append(f'  <pkg-ref id="{PKG_ID}.{p.key}" version="{e(version)}" onConclusion="none">{p.key}.pkg</pkg-ref>')
    out.append("</installer-gui-script>")
    return "\n".join(out) + "\n"


def pkg_resources(res, version):
    res.mkdir(parents=True, exist_ok=True)
    shutil.copy(ROOT / "LICENSE.md", res / "LICENSE.txt")
    style = "<style>body{font-family:-apple-system,Helvetica,sans-serif;font-size:13px;line-height:1.5}</style>"
    (res / "welcome.html").write_text(
        f"<html><head>{style}</head><body><h2>NULL JSFX {html.escape(version)}</h2>"
        "<p>Free and open source audio plugins (MIT). Under <b>Customize</b> you pick the formats "
        "(JSFX for REAPER, JSFX with the NULL look, VST3, CLAP), the groups of plugins and the extras.</p>"
        "<p>Everything goes into your own Library folder, so no password is needed: REAPER's "
        "resource folder, Library/Audio/Plug-Ins/VST3 and CLAP. For one plugin at a time, and for "
        "updates, use the NULL JSFX Hub from <a href=\"https://nulljsfx.tech\">nulljsfx.tech</a>.</p>"
        "</body></html>\n", encoding="utf-8")
    (res / "conclusion.html").write_text(
        f"<html><head>{style}</head><body><h2>Done</h2>"
        "<p>Restart REAPER or your DAW, or rescan its plugins, to see NULL JSFX.</p>"
        "<p>To remove it later, open Library/Application Support/NULL JSFX in your home folder and "
        "double-click <b>Uninstall NULL JSFX.command</b>.</p>"
        "<p>NULL JSFX is free and stays free. If it helps your music, you can support it on "
        "<a href=\"https://ko-fi.com/koulaxizis\">Ko-fi</a>, and follow it there for news of every new version.</p></body></html>\n", encoding="utf-8")


def macos_pkg(args, plugs, chain_list):
    base = (args.pkg_root or args.out / "pkg-build").resolve()
    shutil.rmtree(base, ignore_errors=True)
    tree = pkg_parts(args, plugs, chain_list, base)
    (base / "Distribution.xml").write_text(distribution(tree, args.version), encoding="utf-8")
    pkg_resources(base / "resources", args.version)
    scripts = base / "scripts"
    scripts.mkdir()
    shutil.copy(ROOT / "installer/macos/postinstall", scripts / "postinstall")
    os.chmod(scripts / "postinstall", 0o755)
    count = sum(len(ps) for _, ps in tree)
    if args.pkg_root:
        print(f"build_installer: {base}: {count} parts (no pkgbuild)")
        return
    if not shutil.which("pkgbuild"):
        sys.exit("build_installer: the .pkg needs pkgbuild and productbuild (macOS)")
    comps = base / "components"
    comps.mkdir()
    version = re.sub(r"[^0-9.]", "", args.version) or "0"
    for _, ps in tree:
        for p in ps:
            plist = base / f"{p.key}.plist"
            subprocess.run(["pkgbuild", "--analyze", "--root", p.root, plist], check=True, capture_output=True)
            bundles = plistlib.loads(plist.read_bytes())
            # in place, always: a relocatable bundle would go wherever the system finds one like it
            for b in bundles:
                b["BundleIsRelocatable"] = False
                b["BundleOverwriteAction"] = "upgrade"
            plist.write_bytes(plistlib.dumps(bundles))
            subprocess.run(["pkgbuild", "--root", p.root, "--component-plist", plist, "--identifier", f"{PKG_ID}.{p.key}",
                            "--version", version, "--install-location", "/", "--scripts", scripts,
                            comps / f"{p.key}.pkg"], check=True)
    out = args.out / "NULL-JSFX-Installer-macOS.pkg"
    subprocess.run(["productbuild", "--distribution", base / "Distribution.xml", "--resources", base / "resources",
                    "--package-path", comps, out], check=True)
    print(f"build_installer: {out}: {len(plugs)} plugins, {count} parts, {out.stat().st_size // 1024} KB")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--os", required=True, choices=["Windows", "Linux", "macOS"])
    ap.add_argument("--arch", default=None, choices=["x64", "arm64", "universal"],
                    help="the CPU of the --native package: x64 (default on Windows and Linux), arm64 "
                         "(names the installer ...-ARM64), universal (macOS, the only choice there)")
    ap.add_argument("--jsfx", required=True, type=Path)
    ap.add_argument("--native", required=True, type=Path)
    ap.add_argument("--version", default="dev")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--pkg-root", type=Path, help="macOS: write the .pkg parts here and stop before pkgbuild")
    args = ap.parse_args()
    if args.arch is None:
        args.arch = "universal" if args.os == "macOS" else "x64"
    if (args.os == "macOS") != (args.arch == "universal"):
        sys.exit("build_installer: macOS packages are universal, Windows and Linux ones x64 or arm64")
    plugs, chain_list = plugins(args.jsfx, args.native), chains(args.jsfx)
    stems = {p[0] for p in plugs}
    for folder, name, uses in chain_list:
        missing = set(uses) - stems
        if missing:
            sys.exit(f"build_installer: {folder}/{name} uses unknown plugins: {' '.join(sorted(missing))}")
    (windows if args.os == "Windows" else unix)(args, plugs, chain_list)
    if args.os == "macOS":
        macos_pkg(args, plugs, chain_list)


if __name__ == "__main__":
    main()
