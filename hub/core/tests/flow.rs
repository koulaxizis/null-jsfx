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
    let previous = if v == "2.2" { r#"["2.1"]"# } else { "[]" };
    let catalog = format!(
        r#"{{"schema":1,"version":"{v}","repo":"x/y","packages":{{"jsfx":"NULL-JSFX-JSFX.zip","native":{{"{os}":"NULL-JSFX-VST3-CLAP-{os}.zip"}}}},
        "hub":{{"version":"{v}"}},"previous":{previous},
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
    Targets { reaper: root.join("REAPER"), vst3: root.join("VST3"), clap: root.join("CLAP"), lv2: root.join("LV2"), fonts: root.join("fonts") }
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
    extras_round();
}

/// FX chains follow the plugins, presets merge into the user's own, remote and fonts come and go.
fn extras_round() {
    let files: Files = Default::default();
    let os = null_hub_core::paths::os_name();
    let catalog = format!(
        r#"{{"schema":1,"version":"3.0","repo":"x/y","packages":{{"jsfx":"NULL-JSFX-JSFX.zip","native":{{"{os}":"N.zip"}}}},"hub":{{"version":"1.0"}},
        "plugins":[{{"id":"tilt","name":"Tilt","group":"one-slider","formats":["jsfx","gfx"]}},{{"id":"delay","name":"Delay","group":"essentials","formats":["jsfx","gfx"]}}],
        "chains":[{{"name":"Duo","file":"Duo.RfxChain","plugins":["delay","tilt"],"folders":{{"jsfx":"NULL JSFX","gfx":"NULL JSFX GFX"}}}},
                  {{"name":"Solo","file":"Solo.RfxChain","plugins":["tilt"],"folders":{{"jsfx":"NULL JSFX","gfx":"NULL JSFX GFX"}}}}]}}"#
    );
    let ini = "[General]\nNbPresets=1\n\n[Preset0]\nData=AA\nLen=1\nName=Factory\n\n";
    let j = make_zip(
        "NULL-JSFX-JSFX",
        &[
            ("null_jsfx/tilt.jsfx", &jsfx("Tilt", "3.0")),
            ("null_jsfx/delay.jsfx", &jsfx("Delay", "3.0")),
            ("null_jsfx/gfx/tilt_gfx.jsfx", &jsfx("Tilt GFX", "3.0")),
            ("null_jsfx/gfx/delay_gfx.jsfx", &jsfx("Delay GFX", "3.0")),
            ("null_jsfx/gfx/null_gfx.jsfx-inc", "//"),
            ("FXChains/NULL JSFX/Duo.RfxChain", "duo"),
            ("FXChains/NULL JSFX/Solo.RfxChain", "solo"),
            ("FXChains/NULL JSFX GFX/Duo.RfxChain", "duo gfx"),
            ("FXChains/NULL JSFX GFX/Solo.RfxChain", "solo gfx"),
            ("presets/js-null_jsfx_tilt_jsfx.ini", ini),
            ("presets/js-null_jsfx_delay_jsfx.ini", ini),
            ("Remote/reaper_www_root/null_jsfx.html", "page"),
            ("Remote/Scripts/null_jsfx_remote_bridge.lua", "bridge"),
            ("Remote/INSTALL.txt", "steps"),
            ("fonts/NotoSans-Regular.ttf", "font"),
            ("fonts/NotoSans-Bold.ttf", "font"),
        ],
        false,
    );
    {
        let mut f = files.lock().unwrap();
        f.insert("/latest/download/NULL-JSFX-Catalog.json".into(), catalog.into_bytes());
        f.insert("/download/3.0/NULL-JSFX-JSFX.zip".into(), j);
    }
    std::env::set_var("NULL_JSFX_HUB_RELEASES", serve(files.clone(), true));
    let tmp = tempfile::tempdir().unwrap();
    let root = tmp.path();
    let mut h = hub(root);
    let cat = h.fetch_catalog().unwrap();
    let r = root.join("REAPER");
    // the user's own Tilt preset
    fs::create_dir_all(r.join("presets")).unwrap();
    fs::write(r.join("presets/js-null_jsfx_tilt_jsfx.ini"), "[General]\nNbPresets=1\n\n[Preset0]\nData=BB\nLen=1\nName=Mine\n").unwrap();

    let req = Request {
        install: vec![("tilt".into(), Format::Jsfx)],
        extras: vec!["chains".into(), "presets".into(), "remote".into(), "fonts".into()],
        ..Default::default()
    };
    let s = h.apply(&cat, &req, &mut |_| {}).unwrap();
    assert!(r.join("FXChains/NULL JSFX/Solo.RfxChain").is_file(), "Solo needs only Tilt");
    assert!(!r.join("FXChains/NULL JSFX/Duo.RfxChain").exists(), "Duo needs Delay too");
    assert!(!r.join("FXChains/NULL JSFX GFX").exists(), "no GFX plugins, no GFX chains");
    let mine = fs::read_to_string(r.join("presets/js-null_jsfx_tilt_jsfx.ini")).unwrap();
    let names: Vec<&str> = mine.lines().filter_map(|l| l.strip_prefix("Name=")).collect();
    assert_eq!(names, ["Mine", "Factory"], "{mine}");
    assert!(r.join("reaper_www_root/null_jsfx.html").is_file() && r.join("Scripts/null_jsfx_remote_bridge.lua").is_file());
    assert!(root.join("data/Remote-INSTALL.txt").is_file());
    assert!(root.join("fonts/NotoSans-Bold.ttf").is_file());
    assert!(["chains", "presets", "remote", "fonts"].iter().all(|e| s.extras[*e].installed && !s.extras[*e].outdated));

    // Delay comes: Duo comes along, and Delay gets its presets
    h.apply(&cat, &Request { install: vec![("delay".into(), Format::Jsfx)], ..Default::default() }, &mut |_| {}).unwrap();
    assert!(r.join("FXChains/NULL JSFX/Duo.RfxChain").is_file());
    assert!(r.join("presets/js-null_jsfx_delay_jsfx.ini").is_file());
    // Tilt goes: both chains go, the folder too, and its factory preset; the user's stays.
    // Removing downloads nothing: the package is gone and the kept copy of the presets is enough.
    let pkg = files.lock().unwrap().remove("/download/3.0/NULL-JSFX-JSFX.zip").unwrap();
    let mut downloads = 0;
    h.apply(&cat, &Request { remove: vec![("tilt".into(), Format::Jsfx)], ..Default::default() }, &mut |p| downloads += (p.stage == "download") as u32)
        .unwrap();
    assert_eq!(downloads, 0, "removing must not download");
    files.lock().unwrap().insert("/download/3.0/NULL-JSFX-JSFX.zip".into(), pkg);
    assert!(!r.join("FXChains/NULL JSFX").exists());
    let names = |f: &str| fs::read_to_string(r.join("presets").join(f)).map(|t| t.lines().filter_map(|l| l.strip_prefix("Name=").map(str::to_string)).collect::<Vec<_>>());
    assert_eq!(names("js-null_jsfx_tilt_jsfx.ini").unwrap(), ["Mine"]);
    // remote and fonts off
    let s = h
        .apply(&cat, &Request { extras_remove: vec!["remote".into(), "fonts".into(), "chains".into()], ..Default::default() }, &mut |_| {})
        .unwrap();
    assert!(!r.join("reaper_www_root/null_jsfx.html").exists() && r.join("reaper_www_root").exists(), "REAPER's own folder stays");
    assert!(!root.join("fonts/NotoSans-Bold.ttf").exists());
    assert!(!s.extras["remote"].installed && !s.extras["fonts"].installed && !s.extras["chains"].installed);
    assert!(s.extras["presets"].installed);
    // with chains off, a new plugin brings no chain
    h.apply(&cat, &Request { install: vec![("tilt".into(), Format::Jsfx)], ..Default::default() }, &mut |_| {}).unwrap();
    assert!(!r.join("FXChains").join("NULL JSFX").exists());
    assert_eq!(names("js-null_jsfx_tilt_jsfx.ini").unwrap(), ["Mine", "Factory"], "presets still on: Tilt's comes back");
    // presets off: ours go, the user's stay, a file with only ours goes
    let s = h.apply(&cat, &Request { extras_remove: vec!["presets".into()], ..Default::default() }, &mut |_| {}).unwrap();
    assert_eq!(names("js-null_jsfx_tilt_jsfx.ini").unwrap(), ["Mine"]);
    assert!(!r.join("presets/js-null_jsfx_delay_jsfx.ini").exists());
    assert!(!s.extras["presets"].installed);

    // presets merged before their copies were kept (an older Hub), and no package to read them
    // from: the plugin still goes, its presets file stays as it is
    h.apply(&cat, &Request { extras: vec!["presets".into()], ..Default::default() }, &mut |_| {}).unwrap();
    fs::remove_dir_all(root.join("data/factory-presets")).unwrap();
    let _ = fs::remove_dir_all(root.join("data/cache"));
    files.lock().unwrap().clear();
    let s = h.apply(&cat, &Request { remove: vec![("tilt".into(), Format::Jsfx)], ..Default::default() }, &mut |_| {}).unwrap();
    assert!(!s.plugins["tilt"][&Format::Jsfx].installed);
    assert_eq!(names("js-null_jsfx_tilt_jsfx.ini").unwrap(), ["Mine", "Factory"]);
}

/// Windows: a font file the system or REAPER holds open. Turning the font off says so in plain
/// words, counts it as off, and the Hub removes the file once it is free.
#[cfg(windows)]
#[test]
fn files_in_use_wait_for_later() {
    use std::os::windows::fs::OpenOptionsExt;
    let tmp = tempfile::tempdir().unwrap();
    let root = tmp.path();
    let mut h = hub(root);
    let fonts = root.join("fonts");
    fs::create_dir_all(&fonts).unwrap();
    for f in ["NotoSans-Regular.ttf", "NotoSans-Bold.ttf"] {
        fs::write(fonts.join(f), "font").unwrap();
    }
    let cat: Catalog = serde_json::from_str(
        r#"{"schema":1,"version":"3.0","repo":"x/y","packages":{"jsfx":"J.zip","native":{}},"hub":{"version":"1.0"},"plugins":[]}"#,
    )
    .unwrap();
    assert!(h.scan(&cat).extras["fonts"].installed, "put there by the installer");
    let held = fs::OpenOptions::new().read(true).share_mode(1).open(fonts.join("NotoSans-Bold.ttf")).unwrap();
    let e = h.apply(&cat, &Request { extras_remove: vec!["fonts".into()], ..Default::default() }, &mut |_| {}).unwrap_err();
    assert_eq!(e, null_hub_core::IN_USE);
    assert!(fonts.join("NotoSans-Bold.ttf").exists() && !fonts.join("NotoSans-Regular.ttf").exists());
    assert!(!h.scan(&cat).extras["fonts"].installed, "counts as off while it waits");
    assert_eq!(h.finish_pending(), 1, "still held");
    drop(held);
    // the next start finishes it
    let h = hub(root);
    assert_eq!(h.finish_pending(), 0);
    assert!(!fonts.join("NotoSans-Bold.ttf").exists());
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
        ..Default::default()
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
    files.lock().unwrap().entry("/latest/download/NULL-JSFX-Catalog.json".into()).and_modify(|c| {
        let text = String::from_utf8(c.clone()).unwrap().replace(
            r#"{"id":"delay""#,
            r#"{"id":"reverb","name":"Reverb","group":"essentials","formats":["jsfx"]},{"id":"delay""#,
        );
        *c = text.into_bytes();
    });
    let cat = h.fetch_catalog().unwrap();
    assert_eq!(h.settings.new_ids, vec!["reverb".to_string()], "the new release's new plugins");
    let s = h.scan(&cat);
    assert_eq!((s.installed, s.outdated), (5, 5));
    let note = h.notice(&cat, &s).unwrap();
    assert!(note.starts_with("NULL JSFX 2.2 is out: 2 installed plugins to update, new: Reverb"), "{note}");
    assert_eq!(h.notice(&cat, &s), None, "one notification per release");
    h.settings.notified = None;
    h.settings.language = "el".into();
    let note = h.notice(&cat, &s).unwrap();
    assert!(note.starts_with("Βγήκε το NULL JSFX 2.2: 2 εγκατεστημένα plugins για ενημέρωση, νέα: Reverb"), "{note}");
    h.settings.language = String::new();
    let all: Vec<(String, Format)> = s
        .plugins
        .iter()
        .flat_map(|(id, m)| m.iter().filter(|(_, st)| st.outdated).map(move |(f, _)| (id.clone(), *f)))
        .collect();
    let s = h.apply(&cat, &Request { install: all, remove: vec![], ..Default::default() }, &mut |_| {}).unwrap();
    assert_eq!((s.installed, s.outdated), (5, 0));
    assert_eq!(fs::read_to_string(&bundle).unwrap(), "2.2");
    let cached: Vec<_> = fs::read_dir(root.join("data/cache")).map(|d| d.flatten().map(|e| e.file_name()).collect()).unwrap_or_default();
    assert!(cached.len() <= 1 && cached.iter().all(|n| n == "2.2"), "only the current release stays cached: {cached:?}");

    // check and repair: everything matches; then a damaged bundle file and an edited GFX show up
    assert_eq!(h.verify(&cat, &mut |_| {}).unwrap(), vec![]);
    fs::write(&bundle, "2.X").unwrap();
    fs::write(r.join("gfx/tilt_gfx.jsfx"), jsfx("Tilt GFX", "2.2") + "changed").unwrap();
    let broken = h.verify(&cat, &mut |_| {}).unwrap();
    assert_eq!(broken, vec![("tilt".to_string(), Format::Gfx), ("tilt".to_string(), Format::Vst3)]);
    let s = h.apply(&cat, &Request { install: broken, ..Default::default() }, &mut |_| {}).unwrap();
    assert_eq!((s.installed, s.outdated), (5, 0));
    assert_eq!(fs::read_to_string(&bundle).unwrap(), "2.2");
    assert_eq!(h.verify(&cat, &mut |_| {}).unwrap(), vec![], "repaired");

    // roll Tilt's VST3 back to 2.1: kept there, not offered as an update, not "damaged"
    let back = Request { install: vec![("tilt".into(), Format::Vst3)], from: Some("2.1".into()), ..Default::default() };
    let s = h.apply(&cat, &back, &mut |_| {}).unwrap();
    assert_eq!(fs::read_to_string(&bundle).unwrap(), "2.1");
    let v = &s.plugins["tilt"][&Format::Vst3];
    assert!(v.held && !v.outdated && v.version.as_deref() == Some("2.1"), "{v:?}");
    assert_eq!(s.outdated, 0);
    assert_eq!(h.verify(&cat, &mut |_| {}).unwrap(), vec![]);
    let wrong = Request { install: vec![("tilt".into(), Format::Vst3)], from: Some("1.0".into()), ..Default::default() };
    assert!(h.apply(&cat, &wrong, &mut |_| {}).is_err(), "only the catalog's earlier releases");
    let reverb = Request { install: vec![("reverb".into(), Format::Jsfx)], from: Some("2.1".into()), ..Default::default() };
    assert!(h.apply(&cat, &reverb, &mut |_| {}).unwrap_err().contains("not in release 2.1"));
    assert!(hub(root).settings.held.contains("tilt/vst3"), "the hold is saved");
    // installing it the usual way brings it up to date and lets go of the hold
    let s = h.apply(&cat, &Request { install: vec![("tilt".into(), Format::Vst3)], ..Default::default() }, &mut |_| {}).unwrap();
    assert_eq!(fs::read_to_string(&bundle).unwrap(), "2.2");
    assert!(!s.plugins["tilt"][&Format::Vst3].held && h.settings.held.is_empty());

    // the selection for another computer: what is installed, per plugin
    let file = h.export_selection(&cat, &root.join("out")).unwrap();
    let sel: null_hub_core::Selection = serde_json::from_slice(&fs::read(file).unwrap()).unwrap();
    assert_eq!(sel.kind, "null-jsfx-selection");
    assert_eq!(sel.plugins["tilt"], vec![Format::Jsfx, Format::Gfx, Format::Vst3, Format::Clap]);
    assert_eq!(sel.plugins["delay"], vec![Format::Jsfx]);

    // the records survive a restart
    let h2 = hub(root);
    assert_eq!(h2.scan(&cat).plugins["tilt"][&Format::Vst3].version.as_deref(), Some("2.2"));

    // removing the last GFX version takes the include along; empty folders go too
    let rm: Vec<(String, Format)> = [Format::Jsfx, Format::Gfx, Format::Vst3, Format::Clap].iter().map(|f| ("tilt".to_string(), *f)).collect();
    let s = h.apply(&cat, &Request { install: vec![], remove: rm, ..Default::default() }, &mut |_| {}).unwrap();
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
    let err = h.apply(&cat, &Request { install: vec![("nope".into(), Format::Jsfx)], remove: vec![], ..Default::default() }, &mut |_| {}).unwrap_err();
    assert!(err.contains("not in this release"));
    let _: PathBuf = h.data.clone();
}

/// The page shows the files-in-use message in Greek by its exact English text.
#[test]
fn in_use_message_has_its_greek() {
    let i18n = fs::read_to_string(Path::new(env!("CARGO_MANIFEST_DIR")).join("../ui/i18n.js")).unwrap();
    assert!(i18n.contains(&format!("{:?}:", null_hub_core::IN_USE)), "hub/ui/i18n.js needs the Greek of IN_USE");
}
