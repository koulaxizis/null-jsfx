//! The extras: FX chains, factory presets, the tablet remote and the Noto Sans font. They come
//! from the JSFX package and go where the installers put them (installer/). The Rack is a plugin
//! in the catalog (VST3/CLAP only), so the plugin code handles it.

use crate::catalog::{Catalog, Format};
use crate::paths::Targets;
use std::collections::BTreeSet;
use std::path::PathBuf;

pub const CHAINS: &str = "chains";
pub const PRESETS: &str = "presets";
pub const REMOTE: &str = "remote";
pub const FONTS: &str = "fonts";
pub const ALL: [&str; 4] = [CHAINS, PRESETS, REMOTE, FONTS];

/// A file an extra puts on the computer: (place, path in the JSFX package).
pub type File = (PathBuf, String);

/// The FX chains that fit what will be installed: a chain comes in a JSFX and a GFX version, and
/// each is added when every plugin it uses is installed in that format.
pub fn chain_files(t: &Targets, catalog: &Catalog, installed: &BTreeSet<(String, Format)>) -> Vec<File> {
    let mut out = Vec::new();
    for c in &catalog.chains {
        for (key, folder) in &c.folders {
            let Some(f) = Format::from_key(key) else { continue };
            if c.plugins.iter().all(|p| installed.contains(&(p.clone(), f))) {
                out.push((t.reaper.join("FXChains").join(folder).join(&c.file), format!("FXChains/{folder}/{}", c.file)));
            }
        }
    }
    out
}

/// REAPER's user preset file of a plugin's JSFX or GFX version: (place, path in the package).
pub fn preset_file(t: &Targets, id: &str, f: Format) -> Option<File> {
    let name = match f {
        Format::Jsfx => format!("js-null_jsfx_{id}_jsfx.ini"),
        Format::Gfx => format!("js-null_jsfx_gfx_{id}_gfx_jsfx.ini"),
        _ => return None,
    };
    Some((t.reaper.join("presets").join(&name), format!("presets/{name}")))
}

pub fn remote_files(t: &Targets) -> Vec<File> {
    vec![
        (t.reaper.join("reaper_www_root/null_jsfx.html"), "Remote/reaper_www_root/null_jsfx.html".into()),
        (t.reaper.join("Scripts/null_jsfx_remote_bridge.lua"), "Remote/Scripts/null_jsfx_remote_bridge.lua".into()),
    ]
}

pub fn font_files(t: &Targets) -> Vec<File> {
    ["NotoSans-Regular.ttf", "NotoSans-Bold.ttf"].iter().map(|f| (t.fonts.join(f), format!("fonts/{f}"))).collect()
}

/// Adds the factory presets in `ours` (a REAPER user preset file) to the user's own file
/// `theirs`: the user's presets stay first and untouched, ours follow, and names the user
/// already has are skipped. Same as the installers do.
pub fn merge_presets(theirs: &str, ours: &str) -> String {
    let parse = |text: &str| -> (u64, Vec<(String, String, String)>) {
        let mut n = 0;
        let mut presets = Vec::new();
        let mut cur: Option<(String, String, String)> = None;
        for line in text.lines().map(|l| l.trim_end_matches('\r')) {
            if line.starts_with("[Preset") {
                presets.extend(cur.take());
                cur = Some(Default::default());
            } else if line.starts_with('[') {
                presets.extend(cur.take());
            } else if let Some(v) = line.strip_prefix("NbPresets=") {
                n = v.trim().parse().unwrap_or(0);
            } else if let Some(c) = cur.as_mut() {
                if let Some(v) = line.strip_prefix("Data=") {
                    c.0 = v.to_string();
                } else if let Some(v) = line.strip_prefix("Len=") {
                    c.1 = v.to_string();
                } else if let Some(v) = line.strip_prefix("Name=") {
                    c.2 = v.to_string();
                }
            }
        }
        presets.extend(cur);
        (n, presets)
    };
    let (mut n, have) = parse(theirs);
    let mut names: BTreeSet<String> = have.into_iter().map(|p| p.2).collect();
    let (_, add) = parse(ours);
    let mut extra = String::new();
    for (data, len, name) in add {
        if name.is_empty() || !names.insert(name.clone()) {
            continue;
        }
        extra += &format!("[Preset{n}]\nData={data}\nLen={len}\nName={name}\n\n");
        n += 1;
    }
    if extra.is_empty() {
        return theirs.to_string();
    }
    let mut out = String::new();
    let mut set = false;
    for line in theirs.lines() {
        if line.trim_end_matches('\r').starts_with("NbPresets=") {
            out += &format!("NbPresets={n}\n");
            set = true;
        } else {
            out += line.trim_end_matches('\r');
            out.push('\n');
        }
    }
    if !set {
        out = format!("[General]\nNbPresets={n}\n\n{out}");
    }
    if !out.ends_with("\n\n") {
        out.push('\n');
    }
    out + &extra
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn presets_keep_the_users_first() {
        let mine = "[General]\r\nNbPresets=1\r\n\r\n[Preset0]\r\nData=31202D2074657374E5\r\nLen=8\r\nName=Mine\r\n";
        let ours = "[General]\nNbPresets=2\n\n[Preset0]\nData=AA\nLen=1\nName=Default\n\n[Preset1]\nData=BB\nLen=1\nName=Mine\n\n";
        let out = merge_presets(mine, ours);
        assert!(out.contains("NbPresets=2\n"), "{out}");
        let names: Vec<&str> = out.lines().filter_map(|l| l.strip_prefix("Name=")).collect();
        assert_eq!(names, ["Mine", "Default"]);
        assert!(out.contains("[Preset1]\nData=AA\nLen=1\nName=Default"));
        assert_eq!(merge_presets(&out, ours), out, "merging twice changes nothing");
    }
}
