// NULL JSFX Hub: the window. All the work happens in null_hub_core; this file hands it to the
// page in ui/ (commands below) and reports progress back as "hub-progress" events.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use null_hub_core::{paths, Catalog, Hub, Request, Settings, State, Targets};
use serde::Serialize;
use std::sync::Mutex;
use tauri::{Emitter, Manager};

struct App {
    hub: Mutex<Hub>,
    catalog: Mutex<Option<Catalog>>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct View {
    catalog: Option<Catalog>,
    state: State,
    settings: Settings,
    os: &'static str,
    hub_version: &'static str,
    data_dir: String,
}

fn view(app: &App) -> View {
    let hub = app.hub.lock().unwrap();
    let catalog = app.catalog.lock().unwrap().clone();
    let state = catalog.as_ref().map(|c| hub.scan(c)).unwrap_or_default();
    View {
        catalog,
        state,
        settings: hub.settings.clone(),
        os: paths::os_name(),
        hub_version: env!("CARGO_PKG_VERSION"),
        data_dir: paths::display(&hub.data),
    }
}

/// What the Hub knew last time (works offline).
#[tauri::command]
fn hub_load(app: tauri::State<App>) -> View {
    view(&app)
}

/// Fetches the latest release's catalog.
#[tauri::command]
async fn hub_refresh(handle: tauri::AppHandle) -> Result<View, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let app = handle.state::<App>();
        let catalog = app.hub.lock().unwrap().fetch_catalog()?;
        *app.catalog.lock().unwrap() = Some(catalog);
        Ok(view(&app))
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Installs, updates and removes; progress goes out as "hub-progress" events.
#[tauri::command]
async fn hub_apply(handle: tauri::AppHandle, request: Request) -> Result<View, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let app = handle.state::<App>();
        let catalog = app.catalog.lock().unwrap().clone().ok_or("No catalog yet: connect to the internet and refresh.")?;
        let result = {
            let mut hub = app.hub.lock().unwrap();
            hub.apply(&catalog, &request, &mut |p| {
                let _ = handle.emit("hub-progress", &p);
            })
        };
        result.map(|_| view(&app))
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
fn hub_save_settings(app: tauri::State<App>, settings: Settings) -> Result<View, String> {
    {
        let mut hub = app.hub.lock().unwrap();
        hub.settings = settings;
        hub.save_settings()?;
    }
    Ok(view(&app))
}

#[tauri::command]
fn hub_default_targets() -> Targets {
    Targets::default()
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    // the administrator copy started by fileops::run_elevated (Windows)
    if args.len() == 3 && args[1] == "--apply-ops" {
        std::process::exit(null_hub_core::fileops::main_apply_ops(std::path::Path::new(&args[2])));
    }

    let hub = Hub::open(paths::data_dir());
    let catalog = hub.cached_catalog();
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(App { hub: Mutex::new(hub), catalog: Mutex::new(catalog) })
        .invoke_handler(tauri::generate_handler![hub_load, hub_refresh, hub_apply, hub_save_settings, hub_default_targets])
        .run(tauri::generate_context!())
        .expect("NULL JSFX Hub could not start");
}
