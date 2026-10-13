//! NULL JSFX Hub without its window: reads the catalog of the latest release, finds what is
//! installed on this computer and installs, updates or removes plugins per format, from the
//! release packages on GitHub only. The window (hub/app) and the tests drive it through `Hub`.

pub mod catalog;
pub mod fileops;
pub mod package;
pub mod paths;

pub use catalog::{compare_versions, Catalog, Format};
pub use paths::Targets;

use fileops::Op;
use package::{Package, RemoteZip, Source};
use serde::{Deserialize, Serialize};
use std::cmp::Ordering;
use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};

/// What the Hub installed, with the release each item came from (hub-installed.json).
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct Manifest {
    pub items: BTreeMap<String, Record>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Record {
    pub version: String,
    pub paths: Vec<PathBuf>,
}

fn key(id: &str, f: Format) -> String {
    format!("{id}/{}", f.key())
}

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub targets: Targets,
    /// the formats "Install" adds when the user doesn't pick
    pub formats: Vec<Format>,
    /// look for a new release once a day
    pub check_updates: bool,
    /// show a system notification when one is out
    pub notify: bool,
    /// start with the computer and stay in the tray when the window is closed
    pub autostart: bool,
    /// the plugins that are new in the latest release (shown with a NEW mark)
    pub new_ids: Vec<String>,
    /// the release a notification was already shown for
    pub notified: Option<String>,
    /// when the catalog was last fetched (seconds since 1970)
    pub last_check: u64,
}

impl Default for Settings {
    fn default() -> Self {
        Settings {
            targets: Targets::default(),
            formats: Format::ALL.to_vec(),
            check_updates: true,
            notify: true,
            autostart: false,
            new_ids: Vec::new(),
            notified: None,
            last_check: 0,
        }
    }
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct ItemState {
    pub installed: bool,
    /// the release it came from, when known
    pub version: Option<String>,
    /// installed, but older than the catalog (or of unknown age)
    pub outdated: bool,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct State {
    /// plugin id -> format -> state
    pub plugins: BTreeMap<String, BTreeMap<Format, ItemState>>,
    pub installed: usize,
    pub outdated: usize,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct Request {
    /// (plugin id, format) to install or bring up to date
    #[serde(default)]
    pub install: Vec<(String, Format)>,
    #[serde(default)]
    pub remove: Vec<(String, Format)>,
}

#[derive(Clone, Debug, Serialize)]
pub struct Progress {
    pub stage: &'static str,
    pub text: String,
    pub done: u64,
    pub total: u64,
}

pub struct Hub {
    pub data: PathBuf,
    pub settings: Settings,
    pub manifest: Manifest,
}

fn read_json<T: for<'a> Deserialize<'a> + Default>(path: &Path) -> T {
    fs::read(path).ok().and_then(|b| serde_json::from_slice(&b).ok()).unwrap_or_default()
}

fn write_json<T: Serialize>(path: &Path, value: &T) -> Result<(), String> {
    if let Some(dir) = path.parent() {
        fs::create_dir_all(dir).map_err(|e| format!("{}: {e}", dir.display()))?;
    }
    let tmp = path.with_extension("tmp");
    fs::write(&tmp, serde_json::to_vec_pretty(value).unwrap()).map_err(|e| format!("{}: {e}", tmp.display()))?;
    fs::rename(&tmp, path).map_err(|e| format!("{}: {e}", path.display()))
}

impl Hub {
    pub fn open(data: PathBuf) -> Hub {
        let settings = read_json(&data.join("hub-settings.json"));
        let manifest = read_json(&data.join("hub-installed.json"));
        Hub { data, settings, manifest }
    }

    pub fn save_settings(&self) -> Result<(), String> {
        write_json(&self.data.join("hub-settings.json"), &self.settings)
    }

    fn save_manifest(&self) -> Result<(), String> {
        write_json(&self.data.join("hub-installed.json"), &self.manifest)
    }

    /// The catalog fetched last time, for starting offline.
    pub fn cached_catalog(&self) -> Option<Catalog> {
        Catalog::parse(&fs::read_to_string(self.data.join("catalog.json")).ok()?).ok()
    }

    /// Fetches the latest release's catalog and keeps a copy.
    pub fn fetch_catalog(&mut self) -> Result<Catalog, String> {
        let text = package::fetch_text(&package::catalog_url())?;
        let catalog = Catalog::parse(&text)?;
        // what the new release adds, against the one known so far
        if let Some(old) = self.cached_catalog() {
            if old.version != catalog.version {
                self.settings.new_ids =
                    catalog.plugins.iter().filter(|p| old.plugin(&p.id).is_none()).map(|p| p.id.clone()).collect();
            }
        }
        fs::create_dir_all(&self.data).map_err(|e| e.to_string())?;
        fs::write(self.data.join("catalog.json"), &text).map_err(|e| e.to_string())?;
        self.settings.last_check = now();
        let _ = self.save_settings();
        Ok(catalog)
    }

    /// The text of the system notification for this release, once per release and only when
    /// there is something for the user (updates, new plugins or a new Hub). Marks it as shown.
    pub fn notice(&mut self, catalog: &Catalog, state: &State) -> Option<String> {
        if !self.settings.notify || self.settings.notified.as_deref() == Some(catalog.version.as_str()) {
            return None;
        }
        let mut parts = Vec::new();
        let plugins = state.plugins.iter().filter(|(_, m)| m.values().any(|s| s.outdated)).count();
        if plugins > 0 {
            parts.push(format!("{plugins} installed plugin{} to update", if plugins == 1 { "" } else { "s" }));
        }
        let names: Vec<&str> =
            self.settings.new_ids.iter().filter_map(|id| catalog.plugin(id)).map(|p| p.name.as_str()).collect();
        match names.len() {
            0 => {}
            1..=3 => parts.push(format!("new: {}", names.join(", "))),
            n => parts.push(format!("{n} new plugins")),
        }
        if hub_update(catalog).is_some() {
            parts.push("a new NULL JSFX Hub".into());
        }
        self.settings.notified = Some(catalog.version.clone());
        let _ = self.save_settings();
        if parts.is_empty() {
            return None;
        }
        Some(format!("NULL JSFX {} is out: {}.", catalog.version, parts.join(", ")))
    }

    /// What is installed, per plugin and format, from our records and the folders themselves
    /// (so plugins put there by the installers or by hand count too).
    pub fn scan(&self, catalog: &Catalog) -> State {
        let t = &self.settings.targets;
        let mut installer = None;
        let mut state = State::default();
        for p in &catalog.plugins {
            let mut formats = BTreeMap::new();
            for &f in &p.formats {
                let (dest, _) = paths::plugin_paths(t, &p.id, f);
                let mut s = ItemState::default();
                if dest.exists() {
                    s.installed = true;
                    s.version = match self.manifest.items.get(&key(&p.id, f)) {
                        Some(r) => Some(r.version.clone()),
                        None if f.is_native() => installer.get_or_insert_with(|| installer_version(&self.data)).clone(),
                        None => jsfx_version(&dest),
                    };
                    s.outdated = s.version.as_deref().is_none_or(|v| compare_versions(v, &catalog.version) == Ordering::Less);
                    state.installed += 1;
                    state.outdated += s.outdated as usize;
                }
                formats.insert(f, s);
            }
            state.plugins.insert(p.id.clone(), formats);
        }
        state
    }

    /// Installs, updates and removes what `req` asks for.
    pub fn apply(&mut self, catalog: &Catalog, req: &Request, progress: &mut dyn FnMut(Progress)) -> Result<State, String> {
        let t = self.settings.targets.clone();
        for (id, f) in req.install.iter().chain(&req.remove) {
            let p = catalog.plugin(id).ok_or_else(|| format!("{id} is not in this release"))?;
            if !p.formats.contains(f) {
                return Err(format!("{} has no {} version", p.name, f.key().to_uppercase()));
            }
        }

        // the packages, downloaded once per release
        let cache = self.data.join("cache").join(sanitize(&catalog.version));
        let mut jsfx = None;
        let mut native = None;
        let srcs = |native: bool| -> Vec<String> {
            let mut v: Vec<String> =
                req.install.iter().filter(|(_, f)| f.is_native() == native).map(|(id, f)| paths::plugin_paths(&t, id, *f).1).collect();
            if !native && req.install.iter().any(|(_, f)| *f == Format::Gfx) {
                v.push(paths::gfx_include(&t).1);
            }
            v
        };
        let want = srcs(false);
        if !want.is_empty() {
            jsfx = Some(self.package(catalog, &catalog.packages.jsfx, &want, &cache, progress)?);
        }
        let want = srcs(true);
        if !want.is_empty() {
            let os = paths::os_name();
            let name = catalog.packages.native.get(os).ok_or_else(|| format!("This release has no VST3/CLAP package for {os}"))?;
            native = Some(self.package(catalog, name, &want, &cache, progress)?);
        }
        self.prune_cache(&cache);

        // unpack what is needed into a staging folder, then copy it into place
        let staging = self.data.join("staging");
        let _ = fs::remove_dir_all(&staging);
        let mut ops = Vec::new();
        fn stage(ops: &mut Vec<Op>, staging: &Path, pkg: &mut Source, src: &str, dest: PathBuf, n: usize) -> Result<(), String> {
            let tmp = staging.join(n.to_string()).join(dest.file_name().unwrap_or_default());
            if !pkg.extract(src, &tmp)? {
                return Err(format!("{src} is missing from the release package"));
            }
            ops.push(Op::Copy { from: tmp, to: dest });
            Ok(())
        }
        let total = req.install.len() as u64;
        for (n, (id, f)) in req.install.iter().enumerate() {
            let name = &catalog.plugin(id).unwrap().name;
            let verb = if matches!(if f.is_native() { &native } else { &jsfx }, Some(Source::Remote(_))) { "Downloading" } else { "Unpacking" };
            progress(Progress { stage: "unpack", text: format!("{verb} {name} {}", f.key().to_uppercase()), done: n as u64, total });
            let (dest, src) = paths::plugin_paths(&t, id, *f);
            let pkg = if f.is_native() { native.as_mut() } else { jsfx.as_mut() }.unwrap();
            stage(&mut ops, &staging, pkg, &src, dest, n)?;
        }
        if req.install.iter().any(|(_, f)| *f == Format::Gfx) {
            let (dest, src) = paths::gfx_include(&t);
            stage(&mut ops, &staging, jsfx.as_mut().unwrap(), &src, dest, req.install.len())?;
        }
        for (id, f) in &req.remove {
            let (dest, _) = paths::plugin_paths(&t, id, *f);
            let keep = match f {
                Format::Jsfx | Format::Gfx => t.reaper.join("Effects"),
                Format::Vst3 => t.vst3.clone(),
                Format::Clap => t.clap.clone(),
            };
            ops.push(Op::Remove { path: dest, keep: Some(keep) });
        }

        progress(Progress { stage: "copy", text: "Copying into place".into(), done: 0, total: ops.len() as u64 });
        let failed = fileops::apply(&ops);
        let (denied, mut other): (Vec<_>, Vec<_>) = failed.into_iter().partition(|f| f.denied);
        let mut refused = None;
        if !denied.is_empty() {
            progress(Progress { stage: "elevate", text: "Asking for permission".into(), done: 0, total: 0 });
            let again: Vec<Op> = denied.iter().map(|f| f.op.clone()).collect();
            if let Err(e) = fileops::run_elevated(&again, &staging) {
                refused = Some(e);
                other.extend(denied);
            }
        }
        let _ = fs::remove_dir_all(&staging);
        let failed_paths: Vec<PathBuf> = other
            .iter()
            .map(|f| match &f.op {
                Op::Copy { to, .. } => to.clone(),
                Op::Remove { path, .. } => path.clone(),
            })
            .collect();
        self.record(catalog, req, &failed_paths);

        // the GFX include goes with the last GFX version
        let state = self.scan(catalog);
        let any_gfx = state.plugins.values().any(|m| m.get(&Format::Gfx).is_some_and(|s| s.installed));
        let (inc, _) = paths::gfx_include(&t);
        if !any_gfx && inc.exists() {
            let _ = fileops::apply(&[Op::Remove { path: inc, keep: Some(t.reaper.join("Effects")) }]);
        }
        if let Some(e) = refused {
            return Err(e);
        }
        if !other.is_empty() {
            let text: Vec<String> = other.iter().map(|f| f.error.clone()).collect();
            return Err(format!("Some steps failed: {}", text.join("; ")));
        }
        progress(Progress { stage: "done", text: "Done".into(), done: 1, total: 1 });
        Ok(state)
    }

    /// Notes in hub-installed.json what is now in place (skipping the paths that failed).
    fn record(&mut self, catalog: &Catalog, req: &Request, failed: &[PathBuf]) {
        let t = &self.settings.targets;
        for (id, f) in &req.install {
            let (dest, _) = paths::plugin_paths(t, id, *f);
            if dest.exists() && !failed.contains(&dest) {
                self.manifest.items.insert(key(id, *f), Record { version: catalog.version.clone(), paths: vec![dest] });
            }
        }
        for (id, f) in &req.remove {
            let (dest, _) = paths::plugin_paths(t, id, *f);
            if !dest.exists() {
                self.manifest.items.remove(&key(id, *f));
            }
        }
        let _ = self.save_manifest();
    }

    /// A release package: the copy in the cache, or when the request needs only a small part of
    /// it, just those parts over range requests; otherwise the whole zip, kept for next time.
    fn package(&self, catalog: &Catalog, name: &str, want: &[String], cache: &Path, progress: &mut dyn FnMut(Progress)) -> Result<Source, String> {
        let dest = cache.join(name);
        let url = package::asset_url(&catalog.version, name);
        if !dest.is_file() {
            progress(Progress { stage: "download", text: name.to_string(), done: 0, total: 0 });
            if let Some(remote) = RemoteZip::open(&url)? {
                let need: u64 = want.iter().map(|w| remote.bytes_for(w)).sum();
                if need * 2 < remote.size {
                    return Ok(Source::Remote(remote));
                }
            }
        }
        let mut report = |done, total| progress(Progress { stage: "download", text: name.to_string(), done, total });
        package::download(&url, &dest, &mut report)?;
        match Package::open(&dest) {
            Ok(p) => Ok(Source::Local(p)),
            Err(_) => {
                // a damaged copy: fetch it once more
                let _ = fs::remove_file(&dest);
                package::download(&url, &dest, &mut report)?;
                Package::open(&dest).map(Source::Local)
            }
        }
    }

    /// Keeps only the current release's packages.
    fn prune_cache(&self, keep: &Path) {
        if let Ok(dirs) = fs::read_dir(self.data.join("cache")) {
            for d in dirs.flatten() {
                if d.path() != keep {
                    let _ = fs::remove_dir_all(d.path());
                }
            }
        }
    }
}

/// The newer Hub in the catalog, if this one is older.
pub fn hub_update(catalog: &Catalog) -> Option<String> {
    (compare_versions(&catalog.hub.version, env!("CARGO_PKG_VERSION")) == Ordering::Greater).then(|| catalog.hub.version.clone())
}

fn sanitize(v: &str) -> String {
    v.chars().map(|c| if c.is_ascii_alphanumeric() || c == '.' || c == '-' { c } else { '_' }).collect()
}

pub fn now() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0)
}

/// The release in a JSFX's footer line ("//NULL JSFX v2.1 • https://nulljsfx.tech • MIT License").
pub fn jsfx_version(path: &Path) -> Option<String> {
    let text = fs::read_to_string(path).ok()?;
    let rest = &text[text.find("NULL JSFX v")? + "NULL JSFX v".len()..];
    let v: String = rest.chars().take_while(|c| c.is_ascii_digit() || *c == '.').collect();
    (!v.is_empty()).then_some(v)
}

/// The release the installer put on this computer, for the VST3/CLAP versions it installed.
fn installer_version(data: &Path) -> Option<String> {
    if let Ok(v) = fs::read_to_string(data.join("version.txt")) {
        let v = v.trim();
        if !v.is_empty() {
            return Some(v.to_string());
        }
    }
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        let out = std::process::Command::new("reg")
            .args([
                "query",
                r"HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{287A1049-D196-490D-AD87-B04B0757988D}_is1",
                "/v",
                "DisplayVersion",
            ])
            .creation_flags(0x0800_0000)
            .output()
            .ok()?;
        let text = String::from_utf8_lossy(&out.stdout);
        let line = text.lines().find(|l| l.contains("DisplayVersion"))?;
        return line.split_whitespace().last().map(str::to_string);
    }
    #[allow(unreachable_code)]
    None
}
