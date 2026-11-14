//! The catalog of a release (NULL-JSFX-Catalog.json, made by tools/build_catalog.py).

use serde::{Deserialize, Serialize};
use std::cmp::Ordering;
use std::collections::BTreeMap;

/// The catalog layout this Hub reads; a newer one asks the user to update the Hub.
pub const SCHEMA: u32 = 1;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Format {
    Jsfx,
    Gfx,
    Vst3,
    Clap,
    /// LV2: Linux only, from the same native package (catalog `packages.lv2`)
    Lv2,
    /// Audio Unit (AUv2): macOS only, from the same native package (catalog `packages.au`)
    Au,
}

impl Format {
    pub const ALL: [Format; 6] = [Format::Jsfx, Format::Gfx, Format::Vst3, Format::Clap, Format::Lv2, Format::Au];

    pub fn key(self) -> &'static str {
        match self {
            Format::Jsfx => "jsfx",
            Format::Gfx => "gfx",
            Format::Vst3 => "vst3",
            Format::Clap => "clap",
            Format::Lv2 => "lv2",
            Format::Au => "au",
        }
    }

    pub fn from_key(key: &str) -> Option<Format> {
        Format::ALL.into_iter().find(|f| f.key() == key)
    }

    /// JSFX and GFX live in REAPER; VST3, CLAP, LV2 and AU come from the per-OS native package.
    pub fn is_native(self) -> bool {
        matches!(self, Format::Vst3 | Format::Clap | Format::Lv2 | Format::Au)
    }

    /// The formats offered on this computer: LV2 only on Linux, AU only on macOS.
    pub fn here() -> Vec<Format> {
        let os = crate::paths::os_name();
        Format::ALL
            .into_iter()
            .filter(|&f| (f != Format::Lv2 || os == "Linux") && (f != Format::Au || os == "macOS"))
            .collect()
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Slider {
    pub name: String,
    pub range: String,
    #[serde(default)]
    pub desc: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Plugin {
    pub id: String,
    pub name: String,
    pub group: String,
    #[serde(default)]
    pub tagline: String,
    #[serde(default)]
    pub keywords: String,
    #[serde(default)]
    pub uses: Vec<String>,
    #[serde(default)]
    pub properties: String,
    #[serde(default)]
    pub sliders: Vec<Slider>,
    pub formats: Vec<Format>,
    #[serde(default)]
    pub shot: Option<String>,
    /// the presentation video (an MP4 on the site, its poster next to it as .jpg)
    #[serde(default)]
    pub video: Option<String>,
    #[serde(default)]
    pub guide: Option<String>,
    #[serde(default)]
    pub presets: Vec<String>,
    /// a short demo on the site: the music through this plugin with one of its presets
    #[serde(default)]
    pub demo: Option<Demo>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Demo {
    pub url: String,
    pub preset: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Chain {
    pub name: String,
    pub file: String,
    #[serde(default)]
    pub desc: String,
    pub plugins: Vec<String>,
    /// "jsfx" / "gfx" -> folder under REAPER's FXChains
    pub folders: BTreeMap<String, String>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Group {
    pub id: String,
    pub name: String,
    #[serde(default)]
    pub desc: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Extra {
    pub id: String,
    pub name: String,
    #[serde(default)]
    pub desc: String,
    #[serde(default)]
    pub formats: Vec<Format>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Packages {
    pub jsfx: String,
    /// OS name (Windows, Linux, macOS) -> zip with the VST3 and CLAP versions
    pub native: BTreeMap<String, String>,
    /// the OS names whose native zip also holds an LV2 version of every VST3 plugin (2.7 on).
    /// Plugins list only the formats every Hub knows, so older Hubs keep reading the catalog.
    #[serde(default)]
    pub lv2: Vec<String>,
    /// the OS names whose native zip also holds an Audio Unit of every VST3 plugin except those
    /// in `no_au` (macOS, from 3.0.1 on), the same way as `lv2`
    #[serde(default)]
    pub au: Vec<String>,
    /// the plugins without an Audio Unit (native/scripts/au_skip.txt: auval rejects them)
    #[serde(default)]
    pub no_au: Vec<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct HubInfo {
    pub version: String,
    #[serde(default)]
    pub assets: BTreeMap<String, String>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Catalog {
    pub schema: u32,
    pub version: String,
    #[serde(default)]
    pub date: String,
    #[serde(default)]
    pub notes: String,
    pub repo: String,
    #[serde(default)]
    pub site: String,
    pub packages: Packages,
    pub hub: HubInfo,
    /// earlier releases with the same packages, newest first (to roll a plugin back)
    #[serde(default)]
    pub previous: Vec<String>,
    /// the music of the demos without any plugin
    #[serde(default)]
    pub demo_dry: Option<String>,
    #[serde(default)]
    pub groups: Vec<Group>,
    pub plugins: Vec<Plugin>,
    #[serde(default)]
    pub chains: Vec<Chain>,
    #[serde(default)]
    pub extras: Vec<Extra>,
    /// release file name -> its SHA-256 (hex), from GitHub's digests of the release's assets;
    /// a file downloaded whole is checked against it (catalogs before 3.3.1 have none)
    #[serde(default)]
    pub sha256: BTreeMap<String, String>,
}

/// An id as `tools/build_catalog.py` makes them: lowercase letters, digits, `_` and `-`.
pub fn is_id(s: &str) -> bool {
    !s.is_empty() && s.len() <= 64 && s.bytes().all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'_' || b == b'-')
}

/// A release version: numbers with dots (2.10, 3.3.1), or "dev" for a build from a branch.
pub fn is_version(s: &str) -> bool {
    s == "dev" || (!s.is_empty() && s.len() <= 32 && s.split('.').all(|p| !p.is_empty() && p.bytes().all(|b| b.is_ascii_digit())))
}

/// One plain file or folder name: no separators, no drive, nothing that walks up or hides.
pub fn is_file_name(s: &str) -> bool {
    !s.is_empty()
        && s.len() <= 128
        && !s.starts_with('.')
        && !s.ends_with([' ', '.'])
        && s.chars().all(|c| c.is_ascii_alphanumeric() || matches!(c, ' ' | '.' | '_' | '-' | '+' | '(' | ')'))
}

impl Catalog {
    pub fn parse(text: &str) -> Result<Catalog, String> {
        let probe: serde_json::Value = serde_json::from_str(text).map_err(|e| format!("catalog: {e}"))?;
        let schema = probe.get("schema").and_then(|v| v.as_u64()).unwrap_or(0) as u32;
        if schema > SCHEMA {
            return Err(format!(
                "This catalog needs a newer NULL JSFX Hub (catalog layout {schema}, this Hub reads {SCHEMA}). Download the new Hub from nulljsfx.tech."
            ));
        }
        let mut catalog: Catalog = serde_json::from_value(probe).map_err(|e| format!("catalog: {e}"))?;
        catalog.check()?;
        if catalog.packages.lv2.iter().any(|os| os == crate::paths::os_name()) {
            for p in &mut catalog.plugins {
                if p.formats.contains(&Format::Vst3) && !p.formats.contains(&Format::Lv2) {
                    p.formats.push(Format::Lv2);
                }
            }
        }
        if catalog.packages.au.iter().any(|os| os == crate::paths::os_name()) {
            let skip = catalog.packages.no_au.clone();
            for p in &mut catalog.plugins {
                if p.formats.contains(&Format::Vst3) && !p.formats.contains(&Format::Au) && !skip.contains(&p.id) {
                    p.formats.push(Format::Au);
                }
            }
        }
        Ok(catalog)
    }

    /// Every name the Hub turns into a path or a download address must be a plain name: ids,
    /// versions, package and chain file names, chain folders. Anything else is refused whole.
    fn check(&self) -> Result<(), String> {
        let bad = |what: &str, v: &str| Err(format!("catalog: {what} {v:?} is not allowed; the catalog may be damaged"));
        for v in std::iter::once(&self.version).chain(&self.previous).chain(std::iter::once(&self.hub.version)) {
            if !is_version(v) {
                return bad("version", v);
            }
        }
        for p in &self.plugins {
            if !is_id(&p.id) {
                return bad("plugin id", &p.id);
            }
        }
        for g in &self.groups {
            if !is_id(&g.id) {
                return bad("group id", &g.id);
            }
        }
        for e in &self.extras {
            if !is_id(&e.id) {
                return bad("extra id", &e.id);
            }
        }
        let files = std::iter::once(&self.packages.jsfx).chain(self.packages.native.values()).chain(self.hub.assets.values());
        for f in files {
            if !is_file_name(f) {
                return bad("file", f);
            }
        }
        for c in &self.chains {
            if !is_file_name(&c.file) {
                return bad("FX chain file", &c.file);
            }
            for folder in c.folders.values() {
                if !is_file_name(folder) {
                    return bad("FX chain folder", folder);
                }
            }
            for id in &c.plugins {
                if !is_id(id) {
                    return bad("plugin id", id);
                }
            }
        }
        for (name, hex) in &self.sha256 {
            if !is_file_name(name) || hex.len() != 64 || !hex.bytes().all(|b| b.is_ascii_hexdigit()) {
                return bad("checksum of", name);
            }
        }
        Ok(())
    }

    /// The SHA-256 the release lists for one of its files (lowercase hex), if any.
    pub fn sha256_of(&self, file: &str) -> Option<String> {
        self.sha256.get(file).map(|h| h.to_ascii_lowercase())
    }

    pub fn plugin(&self, id: &str) -> Option<&Plugin> {
        self.plugins.iter().find(|p| p.id == id)
    }
}

/// Compares release versions like 2.1, 2.10, 2.1.1 by their numbers; "dev" and other
/// non-numbers sort before every number.
pub fn compare_versions(a: &str, b: &str) -> Ordering {
    fn parts(v: &str) -> Option<Vec<u64>> {
        v.trim().trim_start_matches(['v', 'V']).split('.').map(|p| p.parse().ok()).collect()
    }
    match (parts(a), parts(b)) {
        (Some(mut x), Some(mut y)) => {
            let n = x.len().max(y.len());
            x.resize(n, 0);
            y.resize(n, 0);
            x.cmp(&y)
        }
        (Some(_), None) => Ordering::Greater,
        (None, Some(_)) => Ordering::Less,
        (None, None) => a.cmp(b),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn versions() {
        assert_eq!(compare_versions("2.10", "2.9"), Ordering::Greater);
        assert_eq!(compare_versions("2.1", "2.1.0"), Ordering::Equal);
        assert_eq!(compare_versions("v2.2", "2.1"), Ordering::Greater);
        assert_eq!(compare_versions("dev", "2.1"), Ordering::Less);
    }

    fn minimal(plugin_id: &str, chain_file: &str, folder: &str, version: &str, jsfx: &str) -> String {
        format!(
            r#"{{"schema": 1, "version": "{version}", "repo": "r", "packages": {{"jsfx": "{jsfx}", "native": {{"Linux": "N.zip"}}}},
            "hub": {{"version": "3.3", "assets": {{"Linux": "NULL-JSFX-Hub-Linux.AppImage"}}}},
            "plugins": [{{"id": "{plugin_id}", "name": "T", "group": "one-slider", "formats": ["jsfx"]}}],
            "chains": [{{"name": "c", "file": "{chain_file}", "plugins": ["tilt"], "folders": {{"jsfx": "{folder}"}}}}]}}"#
        )
    }

    #[test]
    fn names_that_would_leave_their_folder_are_refused() {
        let ok = minimal("tilt", "Mastering.RfxChain", "NULL JSFX", "3.3", "NULL-JSFX-JSFX.zip");
        assert!(Catalog::parse(&ok).is_ok());
        for (id, file, folder, version, jsfx) in [
            ("../../../../home/evil", "a.RfxChain", "NULL JSFX", "3.3", "J.zip"),
            ("Tilt", "a.RfxChain", "NULL JSFX", "3.3", "J.zip"),
            ("tilt", "../../../../home/Documents", "NULL JSFX", "3.3", "J.zip"),
            ("tilt", "a.RfxChain", "../..", "3.3", "J.zip"),
            ("tilt", "a.RfxChain", "x/y", "3.3", "J.zip"),
            ("tilt", "a.RfxChain", "C:", "3.3", "J.zip"),
            ("tilt", "a.RfxChain", "NULL JSFX", "3.3/../../x", "J.zip"),
            ("tilt", "a.RfxChain", "NULL JSFX", "3.3", "..\\\\J.zip"),
            ("tilt", "a.RfxChain", "NULL JSFX", "3.3", ".hidden"),
        ] {
            let text = minimal(id, file, folder, version, jsfx);
            assert!(Catalog::parse(&text).unwrap_err().contains("not allowed"), "{id} {file} {folder} {version} {jsfx}");
        }
        let bad_sum = ok.replacen("\"schema\": 1,", "\"schema\": 1, \"sha256\": {\"J.zip\": \"xyz\"},", 1);
        assert!(Catalog::parse(&bad_sum).is_err());
        let good_sum = ok.replacen("\"schema\": 1,", &format!("\"schema\": 1, \"sha256\": {{\"J.zip\": \"{}\"}},", "A".repeat(64)), 1);
        assert_eq!(Catalog::parse(&good_sum).unwrap().sha256_of("J.zip").unwrap(), "a".repeat(64));
    }

    #[test]
    fn newer_schema_is_refused() {
        let err = Catalog::parse(r#"{"schema": 99}"#).unwrap_err();
        assert!(err.contains("newer NULL JSFX Hub"), "{err}");
    }

    #[test]
    fn au_comes_with_vst3_on_the_listed_os_except_no_au() {
        let text = |au: &str| {
            format!(
                r#"{{"schema": 1, "version": "3.0.1", "repo": "r", "packages": {{"jsfx": "j.zip", "native": {{}}, "au": {au}, "no_au": ["gate"]}},
                "hub": {{"version": "3.0.1"}}, "plugins": [
                {{"id": "tilt", "name": "Tilt", "group": "one-slider", "formats": ["jsfx", "gfx", "vst3", "clap"]}},
                {{"id": "gate", "name": "Gate", "group": "essentials", "formats": ["jsfx", "gfx", "vst3", "clap"]}},
                {{"id": "old", "name": "Old", "group": "advanced", "formats": ["jsfx"]}}]}}"#
            )
        };
        let c = Catalog::parse(&text(&format!("[\"{}\"]", crate::paths::os_name()))).unwrap();
        assert!(c.plugin("tilt").unwrap().formats.contains(&Format::Au));
        assert!(!c.plugin("gate").unwrap().formats.contains(&Format::Au));
        assert!(!c.plugin("old").unwrap().formats.contains(&Format::Au));
        let c = Catalog::parse(&text("[]")).unwrap();
        assert!(!c.plugin("tilt").unwrap().formats.contains(&Format::Au));
        // a catalog from before 3.0.1 has no "au" at all
        let c = Catalog::parse(&text("[]").replace(r#", "au": [], "no_au": ["gate"]"#, "")).unwrap();
        assert!(!c.plugin("tilt").unwrap().formats.contains(&Format::Au));
    }

    #[test]
    fn lv2_comes_with_vst3_on_the_listed_os() {
        let text = |lv2: &str| {
            format!(
                r#"{{"schema": 1, "version": "2.7", "repo": "r", "packages": {{"jsfx": "j.zip", "native": {{}}, "lv2": {lv2}}},
                "hub": {{"version": "2.7"}}, "plugins": [
                {{"id": "tilt", "name": "Tilt", "group": "one-slider", "formats": ["jsfx", "gfx", "vst3", "clap"]}},
                {{"id": "old", "name": "Old", "group": "advanced", "formats": ["jsfx"]}}]}}"#
            )
        };
        let c = Catalog::parse(&text(&format!("[\"{}\"]", crate::paths::os_name()))).unwrap();
        assert!(c.plugin("tilt").unwrap().formats.contains(&Format::Lv2));
        assert!(!c.plugin("old").unwrap().formats.contains(&Format::Lv2));
        let c = Catalog::parse(&text("[]")).unwrap();
        assert!(!c.plugin("tilt").unwrap().formats.contains(&Format::Lv2));
        // a catalog from before 2.7 has no "lv2" at all
        let c = Catalog::parse(&text("[]").replace(r#", "lv2": []"#, "")).unwrap();
        assert!(!c.plugin("tilt").unwrap().formats.contains(&Format::Lv2));
    }
}
