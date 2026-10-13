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
}

impl Format {
    pub const ALL: [Format; 4] = [Format::Jsfx, Format::Gfx, Format::Vst3, Format::Clap];

    pub fn key(self) -> &'static str {
        match self {
            Format::Jsfx => "jsfx",
            Format::Gfx => "gfx",
            Format::Vst3 => "vst3",
            Format::Clap => "clap",
        }
    }

    pub fn from_key(key: &str) -> Option<Format> {
        Format::ALL.into_iter().find(|f| f.key() == key)
    }

    /// JSFX and GFX live in REAPER; VST3 and CLAP come from the per-OS native package.
    pub fn is_native(self) -> bool {
        matches!(self, Format::Vst3 | Format::Clap)
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
    #[serde(default)]
    pub guide: Option<String>,
    #[serde(default)]
    pub presets: Vec<String>,
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
    #[serde(default)]
    pub groups: Vec<Group>,
    pub plugins: Vec<Plugin>,
    #[serde(default)]
    pub chains: Vec<Chain>,
    #[serde(default)]
    pub extras: Vec<Extra>,
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
        serde_json::from_value(probe).map_err(|e| format!("catalog: {e}"))
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

    #[test]
    fn newer_schema_is_refused() {
        let err = Catalog::parse(r#"{"schema": 99}"#).unwrap_err();
        assert!(err.contains("newer NULL JSFX Hub"), "{err}");
    }
}
