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

/// Takes the factory presets in `ours` back out of the user's preset file `theirs`: a preset goes
/// only when its name and data are still exactly ours, so presets the user saved or changed stay.
/// None when no preset is left, so the file can go.
pub fn unmerge_presets(theirs: &str, ours: &str) -> Option<String> {
    // sections in order: (header, body lines); lines before the first header are kept as they are
    let sections = |text: &str| -> (Vec<String>, Vec<(String, Vec<String>)>) {
        let mut head = Vec::new();
        let mut out: Vec<(String, Vec<String>)> = Vec::new();
        for line in text.lines().map(|l| l.trim_end_matches('\r')) {
            if line.starts_with('[') {
                out.push((line.to_string(), Vec::new()));
            } else if let Some(last) = out.last_mut() {
                last.1.push(line.to_string());
            } else {
                head.push(line.to_string());
            }
        }
        (head, out)
    };
    let field = |body: &[String], key: &str| body.iter().find_map(|l| l.strip_prefix(key).map(str::to_string)).unwrap_or_default();
    let is_preset = |h: &str| h.starts_with("[Preset");
    let (_, ours) = sections(ours);
    let factory: BTreeSet<(String, String)> =
        ours.iter().filter(|(h, _)| is_preset(h)).map(|(_, b)| (field(b, "Name="), field(b, "Data="))).collect();
    let (head, have) = sections(theirs);
    let mut n = 0;
    let mut out: Vec<(String, Vec<String>)> = Vec::new();
    for (h, body) in have {
        if !is_preset(&h) {
            out.push((h, body));
        } else if !factory.contains(&(field(&body, "Name="), field(&body, "Data="))) {
            out.push((format!("[Preset{n}]"), body));
            n += 1;
        }
    }
    if n == 0 {
        return None;
    }
    let mut text = head.iter().map(|l| format!("{l}\n")).collect::<String>();
    for (h, body) in out {
        text += &h;
        text.push('\n');
        for l in body {
            let l = if l.starts_with("NbPresets=") { format!("NbPresets={n}") } else { l };
            text += &l;
            text.push('\n');
        }
    }
    Some(text)
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

    #[test]
    fn unmerge_takes_out_only_unchanged_factory_presets() {
        let mine = "[General]\r\nNbPresets=1\r\n\r\n[Preset0]\r\nData=31202D2074657374E5\r\nLen=8\r\nName=Mine\r\n";
        let ours = "[General]\nNbPresets=3\n\n[Preset0]\nData=AA\nLen=1\nName=Default\n\n[Preset1]\nData=BB\nLen=1\nName=Mine\n\n[Preset2]\nData=CC\nLen=1\nName=Wide\n\n";
        let merged = merge_presets(mine, ours);
        // the user changed "Wide" after it came in: it stays
        let merged = merged.replace("Data=CC", "Data=CD");
        let out = unmerge_presets(&merged, ours).unwrap();
        let names: Vec<&str> = out.lines().filter_map(|l| l.strip_prefix("Name=")).collect();
        assert_eq!(names, ["Mine", "Wide"], "{out}");
        assert!(out.contains("NbPresets=2\n") && out.contains("[Preset1]\nData=CD"), "{out}");
        assert_eq!(unmerge_presets(&merge_presets(mine, ours), ours).map(|o| o.matches("[Preset").count()), Some(1));
        // only ours: nothing left, the file can go
        assert_eq!(unmerge_presets(ours, ours), None);
    }
}
