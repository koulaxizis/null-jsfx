//! Updating the Hub itself from the latest release, without the browser: downloads this
//! system's Hub file and puts it in place.
//! Windows: runs the setup in its passive mode (progress bar only), then starts the new Hub.
//! macOS: copies the app out of the disk image over this one and starts it.
//! Linux: replaces the AppImage; a Hub installed from the .deb opens the download instead.

use null_hub_core::{package, paths, Catalog, Progress};
use std::path::{Path, PathBuf};
use std::process::Command;

/// What happened: the new Hub is starting (this one should quit), or the download is only
/// ready to open by hand (the path).
#[cfg_attr(windows, allow(dead_code))]
pub enum Done {
    Restart,
    Open(PathBuf),
}

pub fn run(catalog: &Catalog, data: &Path, progress: &mut dyn FnMut(Progress)) -> Result<Done, String> {
    let file = catalog.hub.assets.get(paths::os_name()).ok_or("This release has no Hub for this system.")?;
    let dir = data.join("hub-update").join(&catalog.hub.version);
    let _ = std::fs::remove_dir_all(data.join("hub-update"));
    let dest = dir.join(file);
    let url = package::asset_url(&catalog.version, file);
    package::download(&url, &dest, &mut |done, total| progress(Progress { stage: "download", text: file.clone(), done, total }))?;
    progress(Progress { stage: "hub", text: "Installing the new NULL JSFX Hub".into(), done: 0, total: 0 });
    install(&dest)
}

#[cfg(windows)]
fn install(setup: &Path) -> Result<Done, String> {
    use std::os::windows::process::CommandExt;
    let exe = std::env::current_exe().map_err(|e| e.to_string())?;
    let q = |p: &Path| p.to_string_lossy().replace('\'', "''");
    // waits for this Hub to quit, runs the setup (/P: passive, no questions), starts the new Hub
    let script = format!(
        "Start-Sleep -Seconds 2; Start-Process -FilePath '{}' -ArgumentList '/P' -Wait; Start-Process -FilePath '{}'",
        q(setup),
        q(&exe)
    );
    Command::new("powershell")
        .args(["-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden", "-Command", &script])
        .creation_flags(0x0800_0000 | 0x0000_0008) // CREATE_NO_WINDOW | DETACHED_PROCESS
        .spawn()
        .map_err(|e| format!("Could not start the Hub setup: {e}"))?;
    Ok(Done::Restart)
}

#[cfg(target_os = "macos")]
fn install(dmg: &Path) -> Result<Done, String> {
    let exe = std::env::current_exe().map_err(|e| e.to_string())?;
    let Some(app) = exe.ancestors().find(|p| p.extension().is_some_and(|e| e == "app")).map(Path::to_path_buf) else {
        return Ok(Done::Open(dmg.to_path_buf()));
    };
    let mount = dmg.with_extension("mount");
    let ok = |c: &mut Command| c.status().map(|s| s.success()).unwrap_or(false);
    if !ok(Command::new("hdiutil").arg("attach").arg(dmg).args(["-nobrowse", "-noautoopen", "-quiet", "-mountpoint"]).arg(&mount)) {
        return Ok(Done::Open(dmg.to_path_buf()));
    }
    let new_app = std::fs::read_dir(&mount)
        .ok()
        .and_then(|d| d.flatten().map(|e| e.path()).find(|p| p.extension().is_some_and(|e| e == "app")));
    let staged = app.with_extension("app-new");
    let old = app.with_extension("app-old");
    let _ = std::fs::remove_dir_all(&staged);
    let _ = std::fs::remove_dir_all(&old);
    let copied = new_app.is_some_and(|n| ok(Command::new("ditto").arg(&n).arg(&staged)));
    let _ = Command::new("hdiutil").arg("detach").arg(&mount).arg("-quiet").status();
    // swap the bundles: the running Hub keeps its open files until it quits
    if !copied || std::fs::rename(&app, &old).is_err() {
        let _ = std::fs::remove_dir_all(&staged);
        return Ok(Done::Open(dmg.to_path_buf()));
    }
    if std::fs::rename(&staged, &app).is_err() {
        let _ = std::fs::rename(&old, &app);
        return Ok(Done::Open(dmg.to_path_buf()));
    }
    let script = format!("sleep 2; rm -rf '{}'; open '{}'", sh(&old), sh(&app));
    Command::new("sh").args(["-c", &script]).spawn().map_err(|e| e.to_string())?;
    Ok(Done::Restart)
}

#[cfg(all(unix, not(target_os = "macos")))]
fn install(image: &Path) -> Result<Done, String> {
    use std::os::unix::fs::PermissionsExt;
    // set by the AppImage runtime: the file this Hub runs from
    let Some(current) = std::env::var_os("APPIMAGE").map(PathBuf::from) else {
        return Ok(Done::Open(image.to_path_buf()));
    };
    let staged = current.with_extension("new");
    std::fs::copy(image, &staged).map_err(|e| format!("{}: {e}", staged.display()))?;
    std::fs::set_permissions(&staged, std::fs::Permissions::from_mode(0o755)).map_err(|e| e.to_string())?;
    // replacing the file is safe while it runs: the running Hub keeps the old one open
    std::fs::rename(&staged, &current).map_err(|e| format!("{}: {e}", current.display()))?;
    let script = format!("sleep 2; exec '{}'", sh(&current));
    Command::new("sh").args(["-c", &script]).spawn().map_err(|e| e.to_string())?;
    Ok(Done::Restart)
}

#[cfg(unix)]
fn sh(p: &Path) -> String {
    p.to_string_lossy().replace('\'', "'\\''")
}
