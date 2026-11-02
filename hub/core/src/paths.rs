//! Where things go on this computer: the same folders the installers use (installer/).

use serde::{Deserialize, Serialize};
use std::env;
use std::path::{Path, PathBuf};

/// The folder of the suite inside the VST3, CLAP and LV2 folders.
pub const VENDOR_DIR: &str = "NULL JSFX";

pub fn os_name() -> &'static str {
    if cfg!(target_os = "windows") {
        "Windows"
    } else if cfg!(target_os = "macos") {
        "macOS"
    } else {
        "Linux"
    }
}

/// True on an ARM64 computer. On Windows the Hub may be an x64 build running under emulation,
/// so the processor is read from the system (PROCESSOR_IDENTIFIER reads "ARMv8 ..." there, and
/// PROCESSOR_ARCHITECTURE is ARM64 for a native process), not from how the Hub was built.
pub fn is_arm64() -> bool {
    if cfg!(target_os = "windows") {
        let var = |k: &str| env::var(k).unwrap_or_default().to_ascii_uppercase();
        var("PROCESSOR_ARCHITECTURE") == "ARM64" || var("PROCESSOR_ARCHITEW6432") == "ARM64" || var("PROCESSOR_IDENTIFIER").starts_with("ARM")
    } else if cfg!(target_os = "macos") {
        false // the macOS packages are universal
    } else {
        cfg!(target_arch = "aarch64")
    }
}

/// The key of the VST3/CLAP package for this computer in the catalog's `native` map: "<OS>-ARM64"
/// on an ARM64 computer when the release has one (from the first release after 2026-11-01),
/// otherwise "<OS>".
pub fn native_key<V>(native: &std::collections::BTreeMap<String, V>) -> String {
    native_key_for(os_name(), is_arm64(), native)
}

pub fn native_key_for<V>(os: &str, arm64: bool, native: &std::collections::BTreeMap<String, V>) -> String {
    let arm = format!("{os}-ARM64");
    if arm64 && native.contains_key(&arm) {
        arm
    } else {
        os.to_string()
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Targets {
    /// REAPER's resource folder (JSFX, GFX, FX chains, presets, remote)
    pub reaper: PathBuf,
    /// the VST3 folder; plugins go into its "NULL JSFX" subfolder
    pub vst3: PathBuf,
    /// the CLAP folder; plugins go into its "NULL JSFX" subfolder
    pub clap: PathBuf,
    /// the LV2 folder (Linux); plugins go into its "NULL JSFX" subfolder. Settings saved by
    /// Hubs before 2.7 have none and get the default.
    #[serde(default = "default_lv2")]
    pub lv2: PathBuf,
    /// the user's font folder
    pub fonts: PathBuf,
}

fn home() -> PathBuf {
    dirs::home_dir().unwrap_or_else(|| PathBuf::from("."))
}

fn default_lv2() -> PathBuf {
    Targets::default().lv2
}

impl Default for Targets {
    fn default() -> Self {
        let h = home();
        if cfg!(target_os = "windows") {
            let appdata = dirs::config_dir().unwrap_or_else(|| h.join("AppData/Roaming"));
            let common = env::var_os("CommonProgramFiles")
                .map(PathBuf::from)
                .unwrap_or_else(|| PathBuf::from(r"C:\Program Files\Common Files"));
            let local = dirs::data_local_dir().unwrap_or_else(|| h.join("AppData/Local"));
            Targets {
                reaper: appdata.join("REAPER"),
                vst3: common.join("VST3"),
                clap: common.join("CLAP"),
                lv2: common.join("LV2"),
                fonts: local.join(r"Microsoft\Windows\Fonts"),
            }
        } else if cfg!(target_os = "macos") {
            Targets {
                reaper: h.join("Library/Application Support/REAPER"),
                vst3: h.join("Library/Audio/Plug-Ins/VST3"),
                clap: h.join("Library/Audio/Plug-Ins/CLAP"),
                lv2: h.join("Library/Audio/Plug-Ins/LV2"),
                fonts: h.join("Library/Fonts"),
            }
        } else {
            let config = env::var_os("XDG_CONFIG_HOME").map(PathBuf::from).unwrap_or_else(|| h.join(".config"));
            let data = env::var_os("XDG_DATA_HOME").map(PathBuf::from).unwrap_or_else(|| h.join(".local/share"));
            Targets { reaper: config.join("REAPER"), vst3: h.join(".vst3"), clap: h.join(".clap"), lv2: h.join(".lv2"), fonts: data.join("fonts") }
        }
    }
}

/// The Hub's own folder: what it installed (hub-installed.json), its settings, the last catalog
/// and the downloaded packages. Shared with the Linux/macOS installer (installed.txt).
pub fn data_dir() -> PathBuf {
    if let Some(dir) = env::var_os("NULL_JSFX_HUB_DATA") {
        return PathBuf::from(dir);
    }
    let h = home();
    if cfg!(target_os = "windows") {
        dirs::config_dir().unwrap_or_else(|| h.join("AppData/Roaming")).join("NULL JSFX")
    } else if cfg!(target_os = "macos") {
        h.join("Library/Application Support/NULL JSFX")
    } else {
        env::var_os("XDG_DATA_HOME").map(PathBuf::from).unwrap_or_else(|| h.join(".local/share")).join("null-jsfx")
    }
}

/// Where a plugin's file (or bundle) goes for a format, and where it sits in the release packages
/// (relative to the package's top folder).
pub fn plugin_paths(t: &Targets, id: &str, format: crate::Format) -> (PathBuf, String) {
    use crate::Format::*;
    match format {
        Jsfx => (effects(t).join(format!("{id}.jsfx")), format!("null_jsfx/{id}.jsfx")),
        Gfx => (effects(t).join("gfx").join(format!("{id}_gfx.jsfx")), format!("null_jsfx/gfx/{id}_gfx.jsfx")),
        Vst3 => (
            t.vst3.join(VENDOR_DIR).join(format!("null_jsfx_{id}.vst3")),
            format!("VST3/{VENDOR_DIR}/null_jsfx_{id}.vst3"),
        ),
        Clap => (
            t.clap.join(VENDOR_DIR).join(format!("null_jsfx_{id}.clap")),
            format!("CLAP/{VENDOR_DIR}/null_jsfx_{id}.clap"),
        ),
        Lv2 => (t.lv2.join(VENDOR_DIR).join(format!("null_jsfx_{id}.lv2")), format!("LV2/{VENDOR_DIR}/null_jsfx_{id}.lv2")),
    }
}

pub fn effects(t: &Targets) -> PathBuf {
    t.reaper.join("Effects").join("null_jsfx")
}

/// The include every GFX version needs.
pub fn gfx_include(t: &Targets) -> (PathBuf, String) {
    (effects(t).join("gfx").join("null_gfx.jsfx-inc"), "null_jsfx/gfx/null_gfx.jsfx-inc".into())
}

pub fn display(p: &Path) -> String {
    p.to_string_lossy().into_owned()
}

#[cfg(test)]
mod tests {
    use super::native_key_for;
    use std::collections::BTreeMap;

    #[test]
    fn native_key_picks_arm64_only_when_the_release_has_it() {
        let old: BTreeMap<String, ()> = ["Windows", "Linux", "macOS"].iter().map(|k| (k.to_string(), ())).collect();
        let mut new = old.clone();
        new.insert("Windows-ARM64".into(), ());
        new.insert("Linux-ARM64".into(), ());
        assert_eq!(native_key_for("Windows", true, &new), "Windows-ARM64");
        assert_eq!(native_key_for("Linux", true, &new), "Linux-ARM64");
        assert_eq!(native_key_for("Windows", false, &new), "Windows");
        assert_eq!(native_key_for("macOS", true, &new), "macOS");
        // a release from before the ARM64 packages: the x64 one, as before
        assert_eq!(native_key_for("Windows", true, &old), "Windows");
    }
}
