//! The whole round: a release on a local server (same layout as GitHub releases), then install,
//! detect, update and remove into temporary folders.

use null_hub_core::{Catalog, Format, Hub, Request, Targets};
use std::collections::BTreeMap;
use std::fs;
use std::io::{BufRead, BufReader, Write};
use std::net::TcpListener;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use zip::write::SimpleFileOptions;

type Files = Arc<Mutex<BTreeMap<String, Vec<u8>>>>;

/// A tiny HTTP server for GET <path>, with or without range requests; returns its base URL.
fn serve(files: Files, ranges: bool) -> String {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let base = format!("http://{}", listener.local_addr().unwrap());
    std::thread::spawn(move || {
        for stream in listener.incoming() {
            let mut stream = stream.unwrap();
            let mut line = String::new();
            let mut reader = BufReader::new(stream.try_clone().unwrap());
            reader.read_line(&mut line).unwrap();
            let mut want = None;
            loop {
                let mut h = String::new();
                if reader.read_line(&mut h).unwrap() == 0 || h == "\r\n" {
                    break;
                }
                if let Some(r) = h.to_ascii_lowercase().strip_prefix("range: bytes=") {
                    let (a, b) = r.trim().split_once('-').unwrap();
                    want = Some((a.parse::<usize>().unwrap(), b.parse::<usize>().unwrap()));
                }
            }
            let path = line.split_whitespace().nth(1).unwrap_or("/").replace("%20", " ");
            let body = files.lock().unwrap().get(&path).cloned();
            let _ = match body {
                Some(b) if ranges && want.is_some() => {
                    let (a, z) = want.unwrap();
                    let z = z.min(b.len() - 1);
                    let _ = write!(
                        stream,
                        "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes {a}-{z}/{}\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
                        b.len(),
                        z + 1 - a
                    );
                    stream.write_all(&b[a..=z])
                }
                Some(b) => {
                    let _ = write!(stream, "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nConnection: close\r\n\r\n", b.len());
                    stream.write_all(&b)
                }
                None => write!(stream, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"),
            };
        }
    });
    base
}

/// A zip with a top folder; `backslash` mimics PowerShell's Compress-Archive.
fn make_zip(top: &str, entries: &[(&str, &str)], backslash: bool) -> Vec<u8> {
    let mut w = zip::ZipWriter::new(std::io::Cursor::new(Vec::new()));
    // a big plugin nobody asks for, so fetching parts pays off
    let mut x: u32 = 1;
    let noise: Vec<u8> = (0..300_000).map(|_| { x = x.wrapping_mul(1_103_515_245).wrapping_add(12_345); (x >> 16) as u8 }).collect();
    w.start_file(format!("{top}/VST3/NULL JSFX/null_jsfx_big.vst3/blob"), SimpleFileOptions::default()).unwrap();
    w.write_all(&noise).unwrap();
    for (name, body) in entries {
        let mut full = format!("{top}/{name}");
        if backslash {
            full = full.replace('/', "\\");
        }
        w.start_file(full, SimpleFileOptions::default()).unwrap();
        w.write_all(body.as_bytes()).unwrap();
    }
    w.finish().unwrap().into_inner()
}

fn jsfx(name: &str, v: &str) -> String {
    format!("desc:NULL {name}\nauthor: x\ntags: y\n//category: One Slider\n//NULL JSFX v{v} • https://nulljsfx.tech • MIT License\n")
}

fn publish(files: &Files, v: &str) {
    let os = null_hub_core::paths::os_name();
    let catalog = format!(
        r#"{{"schema":1,"version":"{v}","repo":"x/y","packages":{{"jsfx":"NULL-JSFX-JSFX.zip","native":{{"{os}":"NULL-JSFX-VST3-CLAP-{os}.zip"}}}},
        "hub":{{"version":"{v}"}},
        "plugins":[
          {{"id":"tilt","name":"Tilt","group":"one-slider","formats":["jsfx","gfx","vst3","clap"]}},
          {{"id":"delay","name":"Delay","group":"essentials","formats":["jsfx","gfx","vst3","clap"]}}]}}"#
    );
    let j = make_zip(
        "NULL-JSFX-JSFX",
        &[
            ("null_jsfx/tilt.jsfx", &jsfx("Tilt", v)),
            ("null_jsfx/delay.jsfx", &jsfx("Delay", v)),
            ("null_jsfx/gfx/tilt_gfx.jsfx", &jsfx("Tilt GFX", v)),
            ("null_jsfx/gfx/delay_gfx.jsfx", &jsfx("Delay GFX", v)),
            ("null_jsfx/gfx/null_gfx.jsfx-inc", "// include"),
        ],
        false,
    );
    let n = make_zip(
        &format!("NULL-JSFX-VST3-CLAP-{os}"),
        &[
            ("VST3/NULL JSFX/null_jsfx_tilt.vst3/Contents/x86_64-linux/null_jsfx_tilt.so", v),
            ("VST3/NULL JSFX/null_jsfx_tilt.vst3/Contents/Resources/info", v),
            ("VST3/NULL JSFX/null_jsfx_delay.vst3/Contents/x86_64-linux/null_jsfx_delay.so", v),
            ("CLAP/NULL JSFX/null_jsfx_tilt.clap", v),
            ("CLAP/NULL JSFX/null_jsfx_delay.clap", v),
        ],
        true,
    );
    let mut f = files.lock().unwrap();
    f.insert("/latest/download/NULL-JSFX-Catalog.json".into(), catalog.into_bytes());
    f.insert(format!("/download/{v}/NULL-JSFX-JSFX.zip"), j);
    f.insert(format!("/download/{v}/NULL-JSFX-VST3-CLAP-{os}.zip"), n);
}

fn targets(root: &Path) -> Targets {
    Targets { reaper: root.join("REAPER"), vst3: root.join("VST3"), clap: root.join("CLAP"), fonts: root.join("fonts") }
}

fn hub(root: &Path) -> Hub {
    let mut h = Hub::open(root.join("data"));
    h.settings.targets = targets(root);
    h
}

#[test]
fn install_update_remove() {
    // one test, so the two servers don't race for NULL_JSFX_HUB_RELEASES
    for ranges in [false, true] {
        round(ranges);
    }
}

fn round(ranges: bool) {
    let files: Files = Default::default();
    publish(&files, "2.1");
    std::env::set_var("NULL_JSFX_HUB_RELEASES", serve(files.clone(), ranges));
    let tmp = tempfile::tempdir().unwrap();
    let root = tmp.path();
    let mut h = hub(root);

    // a JSFX put there earlier by the installer or by hand, from an older release
    let old = root.join("REAPER/Effects/null_jsfx/delay.jsfx");
    fs::create_dir_all(old.parent().unwrap()).unwrap();
    fs::write(&old, jsfx("Delay", "2.0")).unwrap();

    let cat: Catalog = h.fetch_catalog().unwrap();
    assert_eq!(cat.version, "2.1");
    let s = h.scan(&cat);
    let d = &s.plugins["delay"][&Format::Jsfx];
    assert!(d.installed && d.outdated && d.version.as_deref() == Some("2.0"), "{d:?}");
    assert_eq!((s.installed, s.outdated), (1, 1));

    let mut steps = Vec::new();
    let req = Request {
        install: vec![
            ("tilt".into(), Format::Jsfx),
            ("tilt".into(), Format::Gfx),
            ("tilt".into(), Format::Vst3),
            ("tilt".into(), Format::Clap),
            ("delay".into(), Format::Jsfx),
        ],
        remove: vec![],
    };
    let s = h.apply(&cat, &req, &mut |p| steps.push(p.stage)).unwrap();
    assert!(steps.contains(&"download") && steps.last() == Some(&"done"));
    assert_eq!((s.installed, s.outdated), (5, 0));
    let r = root.join("REAPER/Effects/null_jsfx");
    assert!(fs::read_to_string(r.join("delay.jsfx")).unwrap().contains("v2.1"));
    assert!(r.join("gfx/tilt_gfx.jsfx").is_file() && r.join("gfx/null_gfx.jsfx-inc").is_file());
    let bundle = root.join("VST3/NULL JSFX/null_jsfx_tilt.vst3/Contents/Resources/info");
    assert_eq!(fs::read_to_string(&bundle).unwrap(), "2.1", "backslash zip entries unpack as folders");
    let cached = root.join(format!("data/cache/2.1/NULL-JSFX-VST3-CLAP-{}.zip", null_hub_core::paths::os_name()));
    assert_eq!(cached.exists(), !ranges, "with range requests only the needed parts are fetched");
    assert!(root.join("CLAP/NULL JSFX/null_jsfx_tilt.clap").is_file());

    // a new release: everything shows as outdated, then updates in place
    publish(&files, "2.2");
    let cat = h.fetch_catalog().unwrap();
    let s = h.scan(&cat);
    assert_eq!((s.installed, s.outdated), (5, 5));
    let all: Vec<(String, Format)> = s
        .plugins
        .iter()
        .flat_map(|(id, m)| m.iter().filter(|(_, st)| st.outdated).map(move |(f, _)| (id.clone(), *f)))
        .collect();
    let s = h.apply(&cat, &Request { install: all, remove: vec![] }, &mut |_| {}).unwrap();
    assert_eq!((s.installed, s.outdated), (5, 0));
    assert_eq!(fs::read_to_string(&bundle).unwrap(), "2.2");
    let cached: Vec<_> = fs::read_dir(root.join("data/cache")).map(|d| d.flatten().map(|e| e.file_name()).collect()).unwrap_or_default();
    assert!(cached.len() <= 1 && cached.iter().all(|n| n == "2.2"), "only the current release stays cached: {cached:?}");

    // the records survive a restart
    let h2 = hub(root);
    assert_eq!(h2.scan(&cat).plugins["tilt"][&Format::Vst3].version.as_deref(), Some("2.2"));

    // removing the last GFX version takes the include along; empty folders go too
    let rm: Vec<(String, Format)> = Format::ALL.iter().map(|f| ("tilt".to_string(), *f)).collect();
    let s = h.apply(&cat, &Request { install: vec![], remove: rm }, &mut |_| {}).unwrap();
    assert_eq!(s.installed, 1);
    assert!(!r.join("gfx").exists(), "gfx folder with the include must go with the last GFX plugin");
    assert!(!root.join("VST3/NULL JSFX").exists() && root.join("VST3").exists());
    assert!(r.join("delay.jsfx").is_file());
}

#[test]
fn unknown_plugin_is_refused() {
    let tmp = tempfile::tempdir().unwrap();
    let mut h = hub(tmp.path());
    let cat = Catalog::parse(r#"{"schema":1,"version":"2.1","repo":"x","packages":{"jsfx":"a.zip","native":{}},"hub":{"version":"2.1"},"plugins":[]}"#).unwrap();
    let err = h.apply(&cat, &Request { install: vec![("nope".into(), Format::Jsfx)], remove: vec![] }, &mut |_| {}).unwrap_err();
    assert!(err.contains("not in this release"));
    let _: PathBuf = h.data.clone();
}
