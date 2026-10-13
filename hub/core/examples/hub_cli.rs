//! The Hub without a window, for CI and for trying it out:
//!   hub_cli [--root DIR] scan
//!   hub_cli [--root DIR] install tilt:vst3 delay:all ...
//!   hub_cli [--root DIR] remove tilt:all
//!   hub_cli [--root DIR] update
//! --root puts REAPER, VST3, CLAP and fonts under DIR instead of the usual folders.
//! NULL_JSFX_HUB_DATA and NULL_JSFX_HUB_RELEASES work as in the Hub.

use null_hub_core::{paths, Format, Hub, Request, Targets};
use std::path::PathBuf;
use std::process::exit;

fn items(args: &[String]) -> Vec<(String, Format)> {
    let mut out = Vec::new();
    for a in args {
        let (id, f) = a.split_once(':').unwrap_or((a, "all"));
        if f == "all" {
            out.extend(Format::ALL.iter().map(|f| (id.to_string(), *f)));
        } else {
            out.push((id.to_string(), Format::from_key(f).unwrap_or_else(|| fail(&format!("unknown format {f}")))));
        }
    }
    out
}

fn fail(msg: &str) -> ! {
    eprintln!("hub_cli: {msg}");
    exit(1)
}

fn main() {
    let mut args: Vec<String> = std::env::args().skip(1).collect();
    let mut hub = Hub::open(paths::data_dir());
    if args.first().map(String::as_str) == Some("--root") {
        let root = PathBuf::from(args.get(1).cloned().unwrap_or_else(|| fail("--root DIR")));
        hub.settings.targets = Targets { reaper: root.join("REAPER"), vst3: root.join("VST3"), clap: root.join("CLAP"), fonts: root.join("fonts") };
        args.drain(..2);
    }
    let cmd = args.first().cloned().unwrap_or_default();
    let catalog = hub.fetch_catalog().unwrap_or_else(|e| fail(&e));
    let mut req = Request::default();
    match cmd.as_str() {
        "scan" => {}
        "install" => {
            // a plugin without that format is skipped, so "id:all" works for every plugin
            req.install = items(&args[1..]).into_iter().filter(|(id, f)| catalog.plugin(id).is_none_or(|p| p.formats.contains(f))).collect();
        }
        "remove" => {
            let state = hub.scan(&catalog);
            req.remove = items(&args[1..])
                .into_iter()
                .filter(|(id, f)| state.plugins.get(id).and_then(|m| m.get(f)).is_some_and(|s| s.installed))
                .collect();
        }
        "update" => {
            let state = hub.scan(&catalog);
            for (id, m) in &state.plugins {
                req.install.extend(m.iter().filter(|(_, s)| s.outdated).map(|(f, _)| (id.clone(), *f)));
            }
        }
        _ => fail("scan | install ID:FORMAT... | remove ID:FORMAT... | update"),
    }
    if !req.install.is_empty() || !req.remove.is_empty() {
        let mut last = "";
        hub.apply(&catalog, &req, &mut |p| {
            if p.stage != last {
                eprintln!("{}: {}", p.stage, p.text);
                last = p.stage;
            }
        })
        .unwrap_or_else(|e| fail(&e));
    }
    let state = hub.scan(&catalog);
    println!("release {}: {} installed, {} to update", catalog.version, state.installed, state.outdated);
    for (id, m) in &state.plugins {
        let list: Vec<String> = m
            .iter()
            .filter(|(_, s)| s.installed)
            .map(|(f, s)| format!("{}{}{}", f.key(), s.version.as_ref().map(|v| format!("@{v}")).unwrap_or_default(), if s.outdated { "*" } else { "" }))
            .collect();
        if !list.is_empty() {
            println!("  {id}: {}", list.join(" "));
        }
    }
}
