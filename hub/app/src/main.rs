// NULL JSFX Hub: the window, the tray and the notifications. All the work happens in
// null_hub_core; this file hands it to the page in ui/ (commands below), reports progress back as
// "hub-progress" events, and when the user lets it start with the computer, stays in the tray and
// looks for a new release once a day.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use null_hub_core::{hub_update, paths, Catalog, Hub, Request, Settings, State, Targets};
use serde::Serialize;
use std::sync::Mutex;
use std::time::Duration;
use tauri::menu::{Menu, MenuItem};
use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
use tauri::{AppHandle, Emitter, Manager, WindowEvent};
use tauri_plugin_autostart::{MacosLauncher, ManagerExt};
use tauri_plugin_notification::NotificationExt;

/// started by the computer at login: stay in the tray
const HIDDEN: &str = "--hidden";
/// look again when the last check is this old
const CHECK_EVERY: u64 = 20 * 3600;

struct App {
    hub: Mutex<Hub>,
    catalog: Mutex<Option<Catalog>>,
    /// one install/update at a time, also against the daily check
    work: Mutex<()>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct View {
    catalog: Option<Catalog>,
    state: State,
    settings: Settings,
    os: &'static str,
    hub_version: &'static str,
    /// a newer Hub in the latest release, and where to get it
    hub_update: Option<(String, String)>,
    data_dir: String,
}

fn view(app: &App) -> View {
    let hub = app.hub.lock().unwrap();
    let catalog = app.catalog.lock().unwrap().clone();
    let state = catalog.as_ref().map(|c| hub.scan(c)).unwrap_or_default();
    let update = catalog.as_ref().and_then(|c| {
        let v = hub_update(c)?;
        let file = c.hub.assets.get(paths::os_name())?;
        Some((v, format!("https://github.com/{}/releases/latest/download/{}", c.repo, file)))
    });
    View {
        catalog,
        state,
        settings: hub.settings.clone(),
        os: paths::os_name(),
        hub_version: env!("CARGO_PKG_VERSION"),
        hub_update: update,
        data_dir: paths::display(&hub.data),
    }
}

/// Fetches the latest catalog; when the window is hidden, tells the user through a system
/// notification (once per release, only if there is something to do).
fn check(handle: &AppHandle) -> Result<(), String> {
    let app = handle.state::<App>();
    let _busy = app.work.lock().unwrap();
    let catalog = app.hub.lock().unwrap().fetch_catalog()?;
    *app.catalog.lock().unwrap() = Some(catalog.clone());
    let note = {
        let mut hub = app.hub.lock().unwrap();
        let state = hub.scan(&catalog);
        hub.notice(&catalog, &state)
    };
    let visible = handle.get_webview_window("main").and_then(|w| w.is_visible().ok()).unwrap_or(false);
    if let (Some(text), false) = (note, visible) {
        let _ = handle.notification().builder().title("NULL JSFX Hub").body(format!("{text} Open the Hub from the tray to update.")).show();
    }
    let _ = handle.emit("hub-changed", ());
    update_tray(handle);
    Ok(())
}

/// What the Hub knew last time (works offline).
#[tauri::command]
fn hub_load(app: tauri::State<App>) -> View {
    view(&app)
}

/// Fetches the latest release's catalog.
#[tauri::command]
async fn hub_refresh(handle: AppHandle) -> Result<View, String> {
    tauri::async_runtime::spawn_blocking(move || {
        check(&handle)?;
        Ok(view(&handle.state::<App>()))
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Installs, updates and removes; progress goes out as "hub-progress" events.
#[tauri::command]
async fn hub_apply(handle: AppHandle, request: Request) -> Result<View, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let app = handle.state::<App>();
        let _busy = app.work.lock().unwrap();
        let catalog = app.catalog.lock().unwrap().clone().ok_or("No catalog yet: connect to the internet and refresh.")?;
        let result = {
            let mut hub = app.hub.lock().unwrap();
            hub.apply(&catalog, &request, &mut |p| {
                let _ = handle.emit("hub-progress", &p);
            })
        };
        update_tray(&handle);
        result.map(|_| view(&app))
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
fn hub_save_settings(handle: AppHandle, settings: Settings) -> Result<View, String> {
    let app = handle.state::<App>();
    let autostart = settings.autostart;
    {
        let mut hub = app.hub.lock().unwrap();
        hub.settings = settings;
        hub.save_settings()?;
    }
    let launcher = handle.autolaunch();
    let now = launcher.is_enabled().unwrap_or(false);
    if autostart != now {
        let r = if autostart { launcher.enable() } else { launcher.disable() };
        r.map_err(|e| format!("Could not change starting with the computer: {e}"))?;
    }
    Ok(view(&app))
}

#[tauri::command]
fn hub_default_targets() -> Targets {
    Targets::default()
}

fn show(handle: &AppHandle) {
    if let Some(w) = handle.get_webview_window("main") {
        let _ = w.show();
        let _ = w.unminimize();
        let _ = w.set_focus();
    }
}

/// The tray tooltip says how many plugins have an update.
fn update_tray(handle: &AppHandle) {
    let app = handle.state::<App>();
    let n = app.catalog.lock().unwrap().as_ref().map(|c| app.hub.lock().unwrap().scan(c).outdated).unwrap_or(0);
    if let Some(tray) = handle.tray_by_id("hub") {
        let tip = if n > 0 { format!("NULL JSFX Hub: {n} updates") } else { "NULL JSFX Hub".to_string() };
        let _ = tray.set_tooltip(Some(tip));
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    // the administrator copy started by fileops::run_elevated (Windows)
    if args.len() == 3 && args[1] == "--apply-ops" {
        std::process::exit(null_hub_core::fileops::main_apply_ops(std::path::Path::new(&args[2])));
    }
    let hidden = args.iter().any(|a| a == HIDDEN);

    let hub = Hub::open(paths::data_dir());
    let catalog = hub.cached_catalog();
    tauri::Builder::default()
        // a second start (say, from the menu while it sits in the tray) shows the running one
        .plugin(tauri_plugin_single_instance::init(|handle, _, _| show(handle)))
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_notification::init())
        .plugin(tauri_plugin_autostart::init(MacosLauncher::LaunchAgent, Some(vec![HIDDEN])))
        .manage(App { hub: Mutex::new(hub), catalog: Mutex::new(catalog), work: Mutex::new(()) })
        .invoke_handler(tauri::generate_handler![hub_load, hub_refresh, hub_apply, hub_save_settings, hub_default_targets])
        .setup(move |app| {
            let handle = app.handle().clone();
            let open = MenuItem::with_id(app, "open", "Open NULL JSFX Hub", true, None::<&str>)?;
            let look = MenuItem::with_id(app, "check", "Check for updates", true, None::<&str>)?;
            let quit = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
            let menu = Menu::with_items(app, &[&open, &look, &quit])?;
            TrayIconBuilder::with_id("hub")
                .icon(app.default_window_icon().cloned().expect("app icon"))
                .tooltip("NULL JSFX Hub")
                .menu(&menu)
                .show_menu_on_left_click(false)
                .on_menu_event(|handle, event| match event.id.as_ref() {
                    "open" => show(handle),
                    "check" => {
                        let h = handle.clone();
                        std::thread::spawn(move || {
                            let _ = h.state::<App>().hub.lock().map(|mut hub| hub.settings.notified = None);
                            let _ = check(&h);
                        });
                    }
                    "quit" => handle.exit(0),
                    _ => {}
                })
                .on_tray_icon_event(|tray, event| {
                    if let TrayIconEvent::Click { button: MouseButton::Left, button_state: MouseButtonState::Up, .. } = event {
                        show(tray.app_handle());
                    }
                })
                .build(app)?;
            if !hidden {
                show(&handle);
            }
            // the daily look for a new release, while the Hub runs
            std::thread::spawn(move || loop {
                std::thread::sleep(Duration::from_secs(if hidden { 90 } else { 1800 }));
                let due = {
                    let app = handle.state::<App>();
                    let hub = app.hub.lock().unwrap();
                    hub.settings.check_updates && null_hub_core::now().saturating_sub(hub.settings.last_check) >= CHECK_EVERY
                };
                if due {
                    let _ = check(&handle);
                }
            });
            Ok(())
        })
        .on_window_event(|window, event| {
            // with "start with the computer" on, closing the window keeps the Hub in the tray
            if let WindowEvent::CloseRequested { api, .. } = event {
                let keep = window.app_handle().state::<App>().hub.lock().map(|h| h.settings.autostart).unwrap_or(false);
                if keep {
                    api.prevent_close();
                    let _ = window.hide();
                }
            }
        })
        .build(tauri::generate_context!())
        .expect("NULL JSFX Hub could not start")
        .run(|_, _| {});
}
