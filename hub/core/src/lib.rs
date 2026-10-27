//! NULL JSFX Hub without its window: reads the catalog of the latest release, finds what is
//! installed on this computer and installs, updates or removes plugins per format, from the
//! release packages on GitHub only. The window (hub/app) and the tests drive it through `Hub`.

pub mod catalog;
pub mod extras;
pub mod fileops;
pub mod hosts;
pub mod package;
pub mod paths;

pub use catalog::{compare_versions, Catalog, Format};
pub use paths::Targets;

use fileops::Op;
use package::{Package, RemoteZip, Source};
use serde::{Deserialize, Serialize};
use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};
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
    /// plugin/format kept at an earlier release on purpose (rolled back): not offered as updates
    pub held: BTreeSet<String>,
    /// "en" or "el" (the page sets it from the computer's language on the first start)
    pub language: String,
}

impl Default for Settings {
    fn default() -> Self {
        Settings {
            targets: Targets::default(),
            formats: Format::here(),
            check_updates: true,
            notify: true,
            autostart: false,
            new_ids: Vec::new(),
            notified: None,
            last_check: 0,
            held: BTreeSet::new(),
            language: String::new(),
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
    /// rolled back to an earlier release on purpose: kept there, not counted as an update
    #[serde(default)]
    pub held: bool,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct State {
    /// plugin id -> format -> state
    pub plugins: BTreeMap<String, BTreeMap<Format, ItemState>>,
    /// extra id (chains, presets, remote, fonts) -> state
    pub extras: BTreeMap<String, ItemState>,
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
    /// extras to add or bring up to date
    #[serde(default)]
    pub extras: Vec<String>,
    #[serde(default)]
    pub extras_remove: Vec<String>,
    /// install from this earlier release instead (one of the catalog's `previous`): roll back
    #[serde(default)]
    pub from: Option<String>,
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
        let hub = Hub { data, settings, manifest };
        hub.finish_pending();
        hub
    }

    /// Steps left for later because their files were in use (`apply`): the folder that keeps
    /// them, with `ops.json` and the files still to copy.
    fn pending_dir(&self) -> PathBuf {
        self.data.join("pending")
    }

    fn pending(&self) -> Vec<Op> {
        read_json(&self.pending_dir().join("ops.json"))
    }

    /// The files waiting to be removed: they count as gone already.
    fn pending_removals(&self) -> BTreeSet<PathBuf> {
        let mut out = BTreeSet::new();
        for op in self.pending() {
            match op {
                Op::Remove { path, .. } => {
                    out.insert(path);
                }
                Op::Copy { to, .. } => {
                    out.remove(&to);
                }
            }
        }
        out
    }

    /// Keeps the steps that failed because a program held their files, to do once it lets go.
    /// The files to copy move out of the staging folder, which is emptied after each change.
    fn park(&self, ops: Vec<Op>) {
        let dir = self.pending_dir();
        let mut list = self.pending();
        for op in ops {
            let op = match op {
                Op::Copy { from, to } => {
                    let keep = dir.join(format!("{}-{}", now(), list.len())).join(from.file_name().unwrap_or_default());
                    let moved = keep.parent().is_some_and(|d| fs::create_dir_all(d).is_ok()) && fs::rename(&from, &keep).is_ok();
                    if !moved {
                        continue;
                    }
                    Op::Copy { from: keep, to }
                }
                op => op,
            };
            list.push(op);
        }
        let _ = fs::create_dir_all(&dir);
        let _ = write_json(&dir.join("ops.json"), &list);
    }

    /// Tries the steps left for later again (on every start and before every change) and returns
    /// how many are still waiting.
    pub fn finish_pending(&self) -> usize {
        let list = self.pending();
        if list.is_empty() {
            return 0;
        }
        let fonts: Vec<PathBuf> = extras::font_files(&self.settings.targets).into_iter().map(|(d, _)| d).collect();
        let touches_fonts = list.iter().any(|op| match op {
            Op::Copy { to, .. } => fonts.contains(to),
            Op::Remove { path, .. } => fonts.contains(path),
        });
        if touches_fonts {
            fonts_release(&self.settings.targets);
        }
        let failed = fileops::apply(&list);
        if touches_fonts {
            fonts_changed(&self.settings.targets, !fonts.iter().all(|f| f.is_file()));
        }
        let left: Vec<Op> = list.into_iter().filter(|op| failed.iter().any(|f| &f.op == op)).collect();
        let dir = self.pending_dir();
        if left.is_empty() {
            let _ = fs::remove_dir_all(&dir);
        } else {
            let _ = write_json(&dir.join("ops.json"), &left);
        }
        left.len()
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
        let el = self.settings.language == "el";
        let mut parts = Vec::new();
        let plugins = state.plugins.iter().filter(|(_, m)| m.values().any(|s| s.outdated)).count();
        if plugins > 0 {
            parts.push(match (el, plugins) {
                (true, 1) => "1 εγκατεστημένο plugin για ενημέρωση".to_string(),
                (true, n) => format!("{n} εγκατεστημένα plugins για ενημέρωση"),
                (false, n) => format!("{n} installed plugin{} to update", if n == 1 { "" } else { "s" }),
            });
        }
        let names: Vec<&str> =
            self.settings.new_ids.iter().filter_map(|id| catalog.plugin(id)).map(|p| p.name.as_str()).collect();
        match names.len() {
            0 => {}
            1..=3 => parts.push(format!("{}: {}", if el { "νέα" } else { "new" }, names.join(", "))),
            n => parts.push(if el { format!("{n} νέα plugins") } else { format!("{n} new plugins") }),
        }
        if hub_update(catalog).is_some() {
            parts.push(if el { "νέο NULL JSFX Hub" } else { "a new NULL JSFX Hub" }.into());
        }
        self.settings.notified = Some(catalog.version.clone());
        let _ = self.save_settings();
        if parts.is_empty() {
            return None;
        }
        Some(if el {
            format!("Βγήκε το NULL JSFX {}: {}.", catalog.version, parts.join(", "))
        } else {
            format!("NULL JSFX {} is out: {}.", catalog.version, parts.join(", "))
        })
    }

    /// What is installed, per plugin and format, from our records and the folders themselves
    /// (so plugins put there by the installers or by hand count too).
    pub fn scan(&self, catalog: &Catalog) -> State {
        let t = &self.settings.targets;
        let mut installer = None;
        let mut state = State::default();
        let parked = self.pending_removals();
        let exists = |p: &Path| p.exists() && !parked.contains(p);
        for p in &catalog.plugins {
            let mut formats = BTreeMap::new();
            for &f in &p.formats {
                let (dest, _) = paths::plugin_paths(t, &p.id, f);
                let mut s = ItemState::default();
                if exists(&dest) {
                    s.installed = true;
                    s.version = match self.manifest.items.get(&key(&p.id, f)) {
                        Some(r) => Some(r.version.clone()),
                        None if f.is_native() => installer.get_or_insert_with(|| installer_version(&self.data)).clone(),
                        None => jsfx_version(&dest),
                    };
                    s.outdated = s.version.as_deref().is_none_or(|v| compare_versions(v, &catalog.version) == Ordering::Less);
                    if s.outdated && self.settings.held.contains(&key(&p.id, f)) {
                        s.outdated = false;
                        s.held = true;
                    }
                    state.installed += 1;
                    state.outdated += s.outdated as usize;
                }
                formats.insert(f, s);
            }
            state.plugins.insert(p.id.clone(), formats);
        }
        for id in extras::ALL {
            let mut s = ItemState::default();
            if let Some(r) = self.manifest.items.get(&format!("extra/{id}")) {
                s.installed = true;
                s.version = Some(r.version.clone());
            } else {
                // put there by an installer
                s.installed = match id {
                    extras::CHAINS => t.reaper.join("FXChains/NULL JSFX").is_dir() || t.reaper.join("FXChains/NULL JSFX GFX").is_dir(),
                    extras::REMOTE => extras::remote_files(t).iter().all(|(d, _)| exists(d)),
                    extras::FONTS => extras::font_files(t).iter().all(|(d, _)| exists(d)),
                    _ => false,
                };
            }
            if s.installed {
                s.outdated = s.version.as_deref().is_none_or(|v| compare_versions(v, &catalog.version) == Ordering::Less);
                state.outdated += s.outdated as usize;
            }
            state.extras.insert(id.to_string(), s);
        }
        state
    }

    /// Installs, updates and removes what `req` asks for.
    pub fn apply(&mut self, catalog: &Catalog, req: &Request, progress: &mut dyn FnMut(Progress)) -> Result<State, String> {
        let t = self.settings.targets.clone();
        self.finish_pending();
        for (id, f) in req.install.iter().chain(&req.remove) {
            let p = catalog.plugin(id).ok_or_else(|| format!("{id} is not in this release"))?;
            if !p.formats.contains(f) {
                return Err(format!("{} has no {} version", p.name, f.key().to_uppercase()));
            }
        }

        for id in req.extras.iter().chain(&req.extras_remove) {
            if !extras::ALL.contains(&id.as_str()) {
                return Err(format!("unknown extra {id}"));
            }
        }
        // rolling back: plugins only, from one of the earlier releases with the same packages
        let release = match &req.from {
            Some(v) if !catalog.previous.contains(v) => return Err(format!("Release {v} can't be installed from the Hub")),
            Some(_) if !req.remove.is_empty() || !req.extras.is_empty() || !req.extras_remove.is_empty() => {
                return Err("Rolling back takes plugins only".into())
            }
            Some(v) => v.clone(),
            None => catalog.version.clone(),
        };
        let mut plan = if req.from.is_some() { ExtrasPlan::default() } else { self.plan_extras(catalog, req) };
        // the factory presets that were added, kept to take them back out without the package
        let kept = self.data.join("factory-presets");
        let kept_copy = |dest: &Path| kept.join(dest.file_name().unwrap_or_default());

        // the packages, downloaded once per release
        let cache = self.data.join("cache").join(sanitize(&release));
        let mut jsfx = None;
        let mut native = None;
        let srcs = |native: bool| -> Vec<String> {
            let mut v: Vec<String> =
                req.install.iter().filter(|(_, f)| f.is_native() == native).map(|(id, f)| paths::plugin_paths(&t, id, *f).1).collect();
            if !native && req.install.iter().any(|(_, f)| *f == Format::Gfx) {
                v.push(paths::gfx_include(&t).1);
            }
            if !native {
                v.extend(plan.copies.iter().chain(&plan.merges).map(|(_, src)| src.clone()));
                v.extend(plan.unmerges.iter().filter(|(d, _)| !kept_copy(d).is_file()).map(|(_, src)| src.clone()));
            }
            v
        };
        let (want, want_native) = (srcs(false), srcs(true));
        if !want.is_empty() {
            // only presets to take out (plugins installed before their copies were kept): fetch
            // them if we can, but a removal goes ahead without them, leaving the presets file
            let only_unmerges = req.install.iter().all(|(_, f)| f.is_native()) && plan.copies.is_empty() && plan.merges.is_empty();
            match self.package(&release, &catalog.packages.jsfx, &want, &cache, progress) {
                Ok(p) => jsfx = Some(p),
                Err(_) if only_unmerges => plan.unmerges.retain(|(d, _)| kept_copy(d).is_file()),
                Err(e) => return Err(e),
            }
        }
        let want = want_native;
        if !want.is_empty() {
            let os = paths::os_name();
            let name = catalog.packages.native.get(os).ok_or_else(|| format!("This release has no VST3/CLAP package for {os}"))?;
            native = Some(self.package(&release, name, &want, &cache, progress)?);
        }
        self.prune_cache(&cache);

        // unpack what is needed into a staging folder, then copy it into place
        let staging = self.data.join("staging");
        let _ = fs::remove_dir_all(&staging);
        let mut ops = Vec::new();
        let stage = |ops: &mut Vec<Op>, pkg: &mut Source, src: &str, dest: PathBuf, n: usize| -> Result<(), String> {
            let tmp = staging.join(n.to_string()).join(dest.file_name().unwrap_or_default());
            if !pkg.extract(src, &tmp)? {
                return Err(match &req.from {
                    Some(v) => format!("{src} was not in release {v} yet"),
                    None => format!("{src} is missing from the release package"),
                });
            }
            ops.push(Op::Copy { from: tmp, to: dest });
            Ok(())
        };
        let total = req.install.len() as u64;
        for (n, (id, f)) in req.install.iter().enumerate() {
            let name = &catalog.plugin(id).unwrap().name;
            let verb = if matches!(if f.is_native() { &native } else { &jsfx }, Some(Source::Remote(_))) { "Downloading" } else { "Unpacking" };
            progress(Progress { stage: "unpack", text: format!("{verb} {name} {}", f.key().to_uppercase()), done: n as u64, total });
            let (dest, src) = paths::plugin_paths(&t, id, *f);
            let pkg = if f.is_native() { native.as_mut() } else { jsfx.as_mut() }.unwrap();
            stage(&mut ops, pkg, &src, dest, n)?;
        }
        if req.install.iter().any(|(_, f)| *f == Format::Gfx) {
            let (dest, src) = paths::gfx_include(&t);
            stage(&mut ops, jsfx.as_mut().unwrap(), &src, dest, req.install.len())?;
        }
        let mut n = req.install.len() + 1;
        for (dest, src) in &plan.copies {
            n += 1;
            stage(&mut ops, jsfx.as_mut().unwrap(), src, dest.clone(), n)?;
        }
        for (dest, src) in &plan.merges {
            n += 1;
            let tmp = staging.join(n.to_string()).join(dest.file_name().unwrap_or_default());
            if !jsfx.as_mut().unwrap().extract(src, &tmp)? {
                continue; // a plugin without factory presets
            }
            let ours = fs::read_to_string(&tmp).map_err(|e| e.to_string())?;
            let _ = fs::create_dir_all(&kept).and_then(|_| fs::write(kept_copy(dest), &ours));
            let merged = match fs::read_to_string(dest) {
                Ok(theirs) if !theirs.trim().is_empty() => extras::merge_presets(&theirs, &ours),
                _ => ours,
            };
            fs::write(&tmp, merged).map_err(|e| e.to_string())?;
            ops.push(Op::Copy { from: tmp, to: dest.clone() });
        }
        for (dest, src) in &plan.unmerges {
            n += 1;
            let tmp = staging.join(n.to_string()).join(dest.file_name().unwrap_or_default());
            let Ok(theirs) = fs::read_to_string(dest) else { continue };
            let ours = match fs::read_to_string(kept_copy(dest)) {
                Ok(ours) => ours,
                Err(_) => {
                    if !jsfx.as_mut().unwrap().extract(src, &tmp)? {
                        continue;
                    }
                    fs::read_to_string(&tmp).map_err(|e| e.to_string())?
                }
            };
            let _ = fs::remove_file(kept_copy(dest));
            match extras::unmerge_presets(&theirs, &ours) {
                Some(left) if left == theirs => {}
                Some(left) => {
                    if let Some(d) = tmp.parent() {
                        fs::create_dir_all(d).map_err(|e| e.to_string())?;
                    }
                    fs::write(&tmp, left).map_err(|e| e.to_string())?;
                    ops.push(Op::Copy { from: tmp, to: dest.clone() });
                }
                None => ops.push(Op::Remove { path: dest.clone(), keep: dest.parent().map(Path::to_path_buf) }),
            }
        }
        for path in &plan.removes {
            // an emptied NULL JSFX chain folder goes too; REAPER's own folders stay
            let chains = t.reaper.join("FXChains");
            let keep = if path.starts_with(&chains) { chains } else { path.parent().map(Path::to_path_buf).unwrap_or_default() };
            ops.push(Op::Remove { path: path.clone(), keep: Some(keep) });
        }
        for (id, f) in &req.remove {
            let (dest, _) = paths::plugin_paths(&t, id, *f);
            let keep = match f {
                Format::Jsfx | Format::Gfx => t.reaper.join("Effects"),
                Format::Vst3 => t.vst3.clone(),
                Format::Clap => t.clap.clone(),
                Format::Lv2 => t.lv2.clone(),
            };
            ops.push(Op::Remove { path: dest, keep: Some(keep) });
        }

        progress(Progress { stage: "copy", text: "Copying into place".into(), done: 0, total: ops.len() as u64 });
        if plan.fonts_changed {
            // Windows holds a registered font's files: let go of them first
            fonts_release(&t);
        }
        let failed = fileops::apply(&ops);
        let (busy, failed): (Vec<_>, Vec<_>) = failed.into_iter().partition(|f| f.busy);
        let parked = !busy.is_empty();
        if parked {
            self.park(busy.into_iter().map(|f| f.op).collect());
        }
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
        self.record(&release, req, &failed_paths);
        for (id, paths) in &plan.records {
            match paths {
                Some(p) => {
                    self.manifest.items.insert(format!("extra/{id}"), Record { version: catalog.version.clone(), paths: p.clone() });
                }
                None => {
                    self.manifest.items.remove(&format!("extra/{id}"));
                }
            }
        }
        let _ = self.save_manifest();
        if plan.fonts_changed {
            fonts_changed(&t, req.extras_remove.iter().any(|e| e == extras::FONTS));
        }

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
            let running = hosts::running();
            let hint = if running.is_empty() { String::new() } else { format!(" Close {} and try again.", running.join(", ")) };
            return Err(format!("Some steps failed: {}.{hint}", text.join("; ")));
        }
        if parked {
            // the page shows this one in the user's language, so it stays word for word
            return Err(IN_USE.into());
        }
        progress(Progress { stage: "done", text: "Done".into(), done: 1, total: 1 });
        Ok(state)
    }

    /// The installed plugins whose files differ from the release: damaged, changed or missing.
    /// Only those already at the catalog's release are checked (older ones show as updates, and
    /// rolled back ones are at their own release);
    /// installing them again repairs them. Reads the package lists from the cache or, when
    /// there is none, just the zip directories over range requests.
    pub fn verify(&self, catalog: &Catalog, progress: &mut dyn FnMut(Progress)) -> Result<Vec<(String, Format)>, String> {
        let t = &self.settings.targets;
        let items: Vec<(String, Format)> = self
            .scan(catalog)
            .plugins
            .into_iter()
            .flat_map(|(id, m)| m.into_iter().filter(|(_, s)| s.installed && !s.outdated && !s.held).map(move |(f, _)| (id.clone(), f)))
            .collect();
        let cache = self.data.join("cache").join(sanitize(&catalog.version));
        let (mut jsfx, mut native) = (None, None);
        let mut broken = Vec::new();
        for (n, (id, f)) in items.iter().enumerate() {
            let name = &catalog.plugin(id).unwrap().name;
            progress(Progress { stage: "verify", text: format!("{name} {}", f.key().to_uppercase()), done: n as u64, total: items.len() as u64 });
            let slot = if f.is_native() { &mut native } else { &mut jsfx };
            if slot.is_none() {
                let file = if f.is_native() {
                    let os = paths::os_name();
                    catalog.packages.native.get(os).ok_or_else(|| format!("This release has no VST3/CLAP package for {os}"))?
                } else {
                    &catalog.packages.jsfx
                };
                *slot = Some(self.package(&catalog.version, file, &[], &cache, progress)?);
            }
            let (dest, src) = paths::plugin_paths(t, id, *f);
            let files = slot.as_mut().unwrap().files(&src);
            let ok = files.iter().all(|(rel, crc, size)| {
                let path = if rel.is_empty() { dest.clone() } else { dest.join(rel) };
                package::file_matches(&path, *crc, *size)
            });
            if !files.is_empty() && !ok {
                broken.push((id.clone(), *f));
            }
        }
        progress(Progress { stage: "done", text: "Done".into(), done: 1, total: 1 });
        Ok(broken)
    }

    /// What the extras need for this request: FX chains follow the plugins (a chain comes when
    /// all its plugins are in, and goes when one of them goes), factory presets come with each
    /// JSFX/GFX that is installed while they are on.
    fn plan_extras(&self, catalog: &Catalog, req: &Request) -> ExtrasPlan {
        let t = &self.settings.targets;
        let state = self.scan(catalog);
        let mut after: BTreeSet<(String, Format)> = state
            .plugins
            .iter()
            .flat_map(|(id, m)| m.iter().filter(|(_, s)| s.installed).map(move |(f, _)| (id.clone(), *f)))
            .collect();
        after.extend(req.install.iter().cloned());
        for r in &req.remove {
            after.remove(r);
        }
        let on = |id: &str| {
            !req.extras_remove.iter().any(|e| e == id)
                && (req.extras.iter().any(|e| e == id) || state.extras.get(id).is_some_and(|s| s.installed))
        };
        let asked = |id: &str| req.extras.iter().any(|e| e == id);
        let recorded = |id: &str| self.manifest.items.get(&format!("extra/{id}")).map(|r| r.paths.clone()).unwrap_or_default();
        let mut plan = ExtrasPlan::default();

        // FX chains
        let had = recorded(extras::CHAINS);
        if on(extras::CHAINS) {
            let want = extras::chain_files(t, catalog, &after);
            for (dest, src) in &want {
                if asked(extras::CHAINS) || !dest.exists() {
                    plan.copies.push((dest.clone(), src.clone()));
                }
            }
            let keep: BTreeSet<&PathBuf> = want.iter().map(|(d, _)| d).collect();
            plan.removes.extend(had.iter().filter(|p| !keep.contains(p)).cloned());
            if asked(extras::CHAINS) || !had.is_empty() || state.extras[extras::CHAINS].installed {
                plan.records.push((extras::CHAINS.into(), Some(want.into_iter().map(|(d, _)| d).collect())));
            }
        } else if req.extras_remove.iter().any(|e| e == extras::CHAINS) {
            plan.removes.extend(had);
            plan.records.push((extras::CHAINS.into(), None));
        }

        // factory presets: added after the user's own presets; taken back out (only those the
        // user left unchanged) when they are turned off or their plugin goes
        if on(extras::PRESETS) {
            let targets: Vec<(String, Format)> =
                if asked(extras::PRESETS) { after.iter().cloned().collect() } else { req.install.clone() };
            for (id, f) in targets {
                plan.merges.extend(extras::preset_file(t, &id, f));
            }
            if asked(extras::PRESETS) {
                plan.records.push((extras::PRESETS.into(), Some(Vec::new())));
            }
        } else if req.extras_remove.iter().any(|e| e == extras::PRESETS) {
            for p in &catalog.plugins {
                for f in [Format::Jsfx, Format::Gfx] {
                    plan.unmerges.extend(extras::preset_file(t, &p.id, f).filter(|(d, _)| d.is_file()));
                }
            }
            plan.records.push((extras::PRESETS.into(), None));
        }
        if !req.extras_remove.iter().any(|e| e == extras::PRESETS) {
            for (id, f) in &req.remove {
                plan.unmerges.extend(extras::preset_file(t, id, *f).filter(|(d, _)| d.is_file()));
            }
        }

        // the tablet remote and the font: their files, in or out
        for (id, files) in [(extras::REMOTE, extras::remote_files(t)), (extras::FONTS, extras::font_files(t))] {
            if asked(id) {
                plan.copies.extend(files.iter().cloned());
                plan.records.push((id.into(), Some(files.into_iter().map(|(d, _)| d).collect())));
                plan.fonts_changed |= id == extras::FONTS;
                if id == extras::REMOTE {
                    // the steps left to do in REAPER
                    plan.copies.push((self.data.join("Remote-INSTALL.txt"), "Remote/INSTALL.txt".into()));
                }
            } else if req.extras_remove.iter().any(|e| e == id) {
                plan.removes.extend(files.into_iter().map(|(d, _)| d));
                plan.records.push((id.into(), None));
                plan.fonts_changed |= id == extras::FONTS;
            }
        }
        plan
    }

    /// Notes in hub-installed.json what is now in place (skipping the paths that failed).
    /// Also keeps rolled back plugins held at their release, and lets go of the others.
    fn record(&mut self, release: &str, req: &Request, failed: &[PathBuf]) {
        let t = self.settings.targets.clone();
        let held_before = self.settings.held.clone();
        for (id, f) in &req.install {
            let (dest, _) = paths::plugin_paths(&t, id, *f);
            if dest.exists() && !failed.contains(&dest) {
                self.manifest.items.insert(key(id, *f), Record { version: release.to_string(), paths: vec![dest] });
                if req.from.is_some() {
                    self.settings.held.insert(key(id, *f));
                } else {
                    self.settings.held.remove(&key(id, *f));
                }
            }
        }
        let parked = self.pending_removals();
        for (id, f) in &req.remove {
            let (dest, _) = paths::plugin_paths(&t, id, *f);
            if !dest.exists() || parked.contains(&dest) {
                self.manifest.items.remove(&key(id, *f));
                self.settings.held.remove(&key(id, *f));
            }
        }
        let _ = self.save_manifest();
        if self.settings.held != held_before {
            let _ = self.save_settings();
        }
    }

    /// A release package: the copy in the cache, or when the request needs only a small part of
    /// it, just those parts over range requests; otherwise the whole zip, kept for next time.
    fn package(&self, release: &str, name: &str, want: &[String], cache: &Path, progress: &mut dyn FnMut(Progress)) -> Result<Source, String> {
        let dest = cache.join(name);
        let url = package::asset_url(release, name);
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

/// What is installed, to set up another computer the same way (Settings > Export): the plugins
/// with their formats and the extras. No folders, nothing about the computer.
#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct Selection {
    /// "null-jsfx-selection", so the Hub knows the file
    pub kind: String,
    pub release: String,
    pub plugins: BTreeMap<String, Vec<Format>>,
    pub extras: Vec<String>,
}

pub const SELECTION_KIND: &str = "null-jsfx-selection";

impl Hub {
    pub fn selection(&self, catalog: &Catalog) -> Selection {
        let state = self.scan(catalog);
        Selection {
            kind: SELECTION_KIND.into(),
            release: catalog.version.clone(),
            plugins: state
                .plugins
                .into_iter()
                .map(|(id, m)| (id, m.into_iter().filter(|(_, s)| s.installed).map(|(f, _)| f).collect::<Vec<_>>()))
                .filter(|(_, f)| !f.is_empty())
                .collect(),
            extras: state.extras.into_iter().filter(|(_, s)| s.installed).map(|(id, _)| id).collect(),
        }
    }

    /// Writes the selection to `dir`/NULL-JSFX-Selection.json and returns the file.
    pub fn export_selection(&self, catalog: &Catalog, dir: &Path) -> Result<PathBuf, String> {
        let path = dir.join("NULL-JSFX-Selection.json");
        write_json(&path, &self.selection(catalog))?;
        Ok(path)
    }
}

/// The newer Hub in the catalog, if this one is older.
pub fn hub_update(catalog: &Catalog) -> Option<String> {
    (compare_versions(&catalog.hub.version, env!("CARGO_PKG_VERSION")) == Ordering::Greater).then(|| catalog.hub.version.clone())
}

#[derive(Default)]
struct ExtrasPlan {
    /// (place, path in the JSFX package)
    copies: Vec<(PathBuf, String)>,
    /// REAPER preset files to merge ours into: (place, path in the JSFX package)
    merges: Vec<(PathBuf, String)>,
    /// REAPER preset files to take our unchanged presets out of: (place, path in the JSFX package)
    unmerges: Vec<(PathBuf, String)>,
    removes: Vec<PathBuf>,
    /// extra id -> Some(the files it now has) or None (no longer installed)
    records: Vec<(String, Option<Vec<PathBuf>>)>,
    fonts_changed: bool,
}

/// What `apply` returns when some files were in use and their steps wait for the next start.
pub const IN_USE: &str = "Some files are in use by another program, so they could not change yet. \
The Hub finishes this by itself the next time it starts. Close REAPER (for the Noto Sans font, \
sign out of Windows and back in), then open the Hub again.";

#[cfg(windows)]
const FONT_FACES: [(&str, &str); 2] = [("NotoSans-Regular.ttf", "Noto Sans (TrueType)"), ("NotoSans-Bold.ttf", "Noto Sans Bold (TrueType)")];

#[cfg(windows)]
mod winfont {
    #[link(name = "gdi32")]
    extern "system" {
        pub fn AddFontResourceW(file: *const u16) -> i32;
        pub fn RemoveFontResourceW(file: *const u16) -> i32;
    }
    #[link(name = "user32")]
    extern "system" {
        pub fn SendMessageTimeoutW(hwnd: isize, msg: u32, wparam: usize, lparam: isize, flags: u32, timeout: u32, result: *mut usize) -> isize;
    }

    pub fn wide(p: &std::path::Path) -> Vec<u16> {
        use std::os::windows::ffi::OsStrExt;
        p.as_os_str().encode_wide().chain(Some(0)).collect()
    }

    /// WM_FONTCHANGE to every window, so programs see the font list changed.
    pub fn broadcast() {
        let mut r = 0;
        unsafe { SendMessageTimeoutW(0xffff, 0x001D, 0, 0, 0x0002, 1000, &mut r) };
    }
}

/// Windows keeps the files of a registered font open from sign-in on, so before they are
/// replaced or removed the Hub takes the font out of the session and the user's font list.
/// `fonts_changed` puts it back when the files stay.
fn fonts_release(t: &Targets) {
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        let key = r"HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts";
        for (file, face) in FONT_FACES {
            let w = winfont::wide(&t.fonts.join(file));
            // once per time it was added
            for _ in 0..8 {
                if unsafe { winfont::RemoveFontResourceW(w.as_ptr()) } == 0 {
                    break;
                }
            }
            let _ = std::process::Command::new("reg").args(["delete", key, "/v", face, "/f"]).creation_flags(0x0800_0000).status();
        }
        winfont::broadcast();
    }
    #[cfg(not(windows))]
    let _ = t;
}

/// Tells the system about the Noto Sans files that came or went: the user font list on Windows,
/// the font cache on Linux. macOS picks them up by itself.
fn fonts_changed(t: &Targets, removed: bool) {
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        let key = r"HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts";
        for (file, face) in FONT_FACES {
            let path = t.fonts.join(file);
            if removed || !path.is_file() {
                let _ = std::process::Command::new("reg").args(["delete", key, "/v", face, "/f"]).creation_flags(0x0800_0000).status();
            } else {
                let _ = std::process::Command::new("reg")
                    .args(["add", key, "/v", face, "/t", "REG_SZ", "/d", &path.to_string_lossy(), "/f"])
                    .creation_flags(0x0800_0000)
                    .status();
                unsafe { winfont::AddFontResourceW(winfont::wide(&path).as_ptr()) };
            }
        }
        winfont::broadcast();
    }
    #[cfg(target_os = "linux")]
    {
        let _ = removed;
        let _ = std::process::Command::new("fc-cache").arg("-f").arg(&t.fonts).status();
    }
    #[cfg(target_os = "macos")]
    let _ = (t, removed);
}

fn sanitize(v: &str) -> String {
    v.chars().map(|c| if c.is_ascii_alphanumeric() || c == '.' || c == '-' { c } else { '_' }).collect()
}

pub fn now() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0)
}

/// The release in a JSFX's footer line ("//NULL JSFX v2.6 • https://nulljsfx.tech • MIT License").
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
