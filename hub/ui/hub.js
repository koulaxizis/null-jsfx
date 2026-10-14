// NULL JSFX Hub page. Talks to the Rust side (hub/app/src/main.rs) through Tauri's invoke and
// the "hub-progress" event; the page itself never touches the network or the disk.
"use strict";

const T = window.__TAURI__;
const invoke = (cmd, args) => T.core.invoke(cmd, args);
const FORMATS = [
  ["jsfx", "JSFX", "REAPER, with REAPER's own sliders"],
  ["gfx", "GFX", "REAPER, with the NULL JSFX interface"],
  ["vst3", "VST3", "any DAW"],
  ["clap", "CLAP", "any DAW"],
];
const $ = (id) => document.getElementById(id);

let V = null; // the view from Rust: catalog, state, settings, os, hubVersion, dataDir
let view = "all";
let busy = false;
let detailId = null;

function esc(s) {
  return String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function plugins() { return V?.catalog?.plugins ?? []; }
/** the effects; the Rack (group "extras") is listed under Extras */
function effects() { return plugins().filter((p) => p.group !== "extras"); }
function extraSt(id) { return V.state.extras?.[id] ?? {}; }
function outdatedExtras() { return Object.entries(V.state.extras ?? {}).filter(([, s]) => s.outdated).map(([id]) => id); }
function st(id) { return V.state.plugins[id] ?? {}; }
function installedFormats(id) { return Object.entries(st(id)).filter(([, s]) => s.installed).map(([f]) => f); }
function outdatedFormats(id) { return Object.entries(st(id)).filter(([, s]) => s.outdated).map(([f]) => f); }
function isInstalled(id) { return installedFormats(id).length > 0; }
function heldFormats(id) { return Object.entries(st(id)).filter(([, s]) => s.held).map(([f]) => f); }
/** installed at the latest release */
function currentFormats(id) { return Object.entries(st(id)).filter(([, s]) => s.installed && !s.outdated && !s.held).map(([f]) => f); }
function isNew(id) { return (V.settings.new_ids || []).includes(id); }

// --- talking to Rust -----------------------------------------------------------------------

/** VST3/CLAP files change: a running DAW keeps them (Windows can't replace them at all) */
function touchesNative(request) {
  return [...(request.install || []), ...(request.remove || [])].some(([, f]) => f === "vst3" || f === "clap");
}

async function run(request, doneText, checked) {
  if (busy) return;
  if (!checked && touchesNative(request)) {
    let hosts = [];
    try { hosts = await invoke("hub_hosts"); } catch (_) { /* can't tell: go ahead */ }
    if (hosts.length) {
      const names = hosts.join(tr(" and "));
      const why = V.os === "Windows"
        ? tr("Windows does not let a DAW's loaded VST3 and CLAP plugins be replaced or removed until it closes.")
        : tr("A DAW picks up new VST3 and CLAP files only after a restart.");
      return ask(tr("Close {names} first?", { names }),
        `${tr(hosts.length === 1 ? "{names} is running." : "{names} are running.", { names })} ${why} ${tr("Save your projects, close {names}, then continue.", { names })}`,
        tr("Continue"), () => run(request, doneText, true), true);
    }
  }
  busy = true;
  render();
  showProgress(tr("Starting"), -1);
  try {
    V = await invoke("hub_apply", { request });
    toast(doneText);
  } catch (e) {
    toast(String(e), true);
    try { V = await invoke("hub_load"); } catch (_) { /* keep the old view */ }
  } finally {
    busy = false;
    $("progress").hidden = true;
    render();
    if (detailId) openDetail(detailId);
  }
}

async function refresh(quiet) {
  $("refresh").disabled = true;
  $("refresh").textContent = tr("Checking");
  try {
    V = await invoke("hub_refresh");
    if (!quiet) toast(V.state.outdated ? tr("{n} updates for release {v}", { n: V.state.outdated, v: V.catalog.version }) : tr("Up to date: release {v}", { v: V.catalog.version }));
  } catch (e) {
    toast(V?.catalog ? `${e} ${tr("Showing what the Hub knew last time.")}` : String(e), true);
  } finally {
    $("refresh").disabled = false;
    $("refresh").textContent = tr("Check for updates");
    render();
  }
}

function showProgress(text, frac) {
  $("progress").hidden = false;
  $("progress-text").textContent = text;
  const fill = $("progress-fill");
  fill.classList.toggle("busy", frac < 0);
  fill.style.width = frac < 0 ? "" : `${Math.round(frac * 100)}%`;
}

function onProgress(p) {
  const mb = (n) => (n / 1048576).toFixed(1);
  if (p.stage === "download") {
    const size = p.total ? tr("{done} of {total} MB", { done: mb(p.done), total: mb(p.total) }) : `${mb(p.done)} MB`;
    showProgress(`${tr("Downloading {what}", { what: p.text })}: ${size}`, p.total ? p.done / p.total : -1);
  } else if (p.stage === "unpack") {
    // the Rust side says "Downloading/Unpacking <name> <FORMAT>"
    const [verb, ...rest] = p.text.split(" ");
    showProgress(tr(verb === "Downloading" ? "Downloading {what}" : "Unpacking {what}", { what: rest.join(" ") }), p.total ? p.done / p.total : -1);
  } else if (p.stage === "verify") {
    showProgress(tr("Checking {what}", { what: p.text }), p.total ? p.done / p.total : -1);
  } else if (p.stage === "elevate") {
    showProgress(tr("Windows asks for permission to write to the VST3/CLAP folders"), -1);
  } else {
    showProgress(tr(p.text), -1);
  }
}

let toastTimer;
function toast(text, bad) {
  const t = $("toast");
  t.textContent = text;
  t.classList.toggle("bad", !!bad);
  t.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => (t.hidden = true), bad ? 9000 : 4000);
}

// --- requests --------------------------------------------------------------------------------

function wanted(p) {
  const pick = V.settings.formats.filter((f) => p.formats.includes(f));
  return pick.length ? pick : p.formats.slice(0, 1);
}

/** the formats picked in the bar (Settings) that this plugin has but are not installed yet */
function missing(p) {
  return wanted(p).filter((f) => !st(p.id)[f]?.installed);
}

function installReq(list) {
  return { install: list.flatMap((p) => missing(p).map((f) => [p.id, f])), remove: [] };
}

/** every format of these plugins that is installed */
function removeReq(list) {
  return { install: [], remove: list.flatMap((p) => installedFormats(p.id).map((f) => [p.id, f])) };
}

function label(f) { return FORMATS.find(([k]) => k === f)?.[1] ?? f; }
function plural(n, one, many) { return `${n} ${tr(n === 1 ? one : many)}`; }

function updateReq(list, extras = []) {
  return { install: list.flatMap((p) => outdatedFormats(p.id).map((f) => [p.id, f])), remove: [], extras, extras_remove: [] };
}

// --- rendering -------------------------------------------------------------------------------

function shown() {
  const q = $("search").value.trim().toLowerCase();
  return (view === "extras" || view === "installed" || view === "updates" ? plugins() : effects()).filter((p) => {
    if (view === "extras" && p.group !== "extras") return false;
    if (view === "installed" && !isInstalled(p.id)) return false;
    if (view === "updates" && !outdatedFormats(p.id).length) return false;
    if (view === "new" && !isNew(p.id)) return false;
    if (view.startsWith("group:") && p.group !== view.slice(6)) return false;
    if (!q) return true;
    return q.split(/\s+/).every((w) => `${p.name} ${p.tagline} ${p.keywords}`.toLowerCase().includes(w));
  });
}

function shotHtml(p, big) {
  // the plugin page plays the presentation video, with its poster until it starts
  if (big && p.video) return `<video src="${esc(p.video)}" poster="${esc(p.video.replace(/\.mp4$/, ".jpg"))}" controls playsinline preload="none"></video>`;
  return p.shot
    ? `<img src="${esc(p.shot)}" alt="" ${big ? "" : 'loading="lazy"'}>`
    : "NULL";
}

function chips(p) {
  const s = st(p.id);
  return (isNew(p.id) ? `<span class="chip new" title="${tr("New in this release")}">${tr("NEW")}</span>` : "") + FORMATS.filter(([f]) => p.formats.includes(f))
    .map(([f, label]) => {
      const x = s[f] || {};
      const cls = x.outdated ? "chip old" : x.installed ? "chip on" : "chip";
      const title = `${label}: ${x.outdated ? tr("update available") : x.held ? tr("kept at {v}", { v: x.version }) : x.installed ? tr("installed") : tr("not installed")}`;
      return `<span class="${cls}" title="${title}">${label}</span>`;
    })
    .join("");
}

function cardButton(p) {
  const remove = isInstalled(p.id) ? `<button class="ghost mini" data-act="remove" data-id="${p.id}" title="${esc(tr("Remove {name} (every installed format)", { name: p.name }))}" aria-label="${esc(tr("Remove {name}", { name: p.name }))}">&times;</button>` : "";
  if (outdatedFormats(p.id).length) return `${remove}<button data-act="update" data-id="${p.id}">${tr("Update")}</button>`;
  const add = missing(p);
  if (add.length && isInstalled(p.id)) return `${remove}<button data-act="install" data-id="${p.id}" title="${esc(tr("Install the {formats} version", { formats: add.map(label).join(tr(" and ")) }))}">+ ${add.map(label).join(" ")}</button>`;
  if (add.length) return `<button data-act="install" data-id="${p.id}">${tr("Install")}</button>`;
  return `${remove}<button class="done" data-act="open" data-id="${p.id}">${tr("Installed")}</button>`;
}

function render() {
  if (!V) return;
  const cat = V.catalog;
  const s = V.state;
  $("n-all").textContent = effects().length || "";
  const extrasOn = Object.values(s.extras ?? {}).filter((x) => x.installed).length + plugins().filter((p) => p.group === "extras" && isInstalled(p.id)).length;
  $("n-extras").textContent = extrasOn || "";
  $("n-installed").textContent = Object.keys(s.plugins).filter(isInstalled).length || "";
  const updPlugins = plugins().filter((p) => outdatedFormats(p.id).length).length;
  const upd = updPlugins + outdatedExtras().length;
  $("n-updates").textContent = updPlugins || "";
  const fresh = plugins().filter((p) => isNew(p.id)).length;
  $("nav-new").hidden = !fresh;
  $("n-new").textContent = fresh || "";
  const hu = $("hub-update");
  hu.hidden = !V.hubUpdate;
  if (V.hubUpdate) hu.innerHTML = `<span>${esc(tr("A new NULL JSFX Hub ({v}) is out.", { v: V.hubUpdate[0] }))}</span><button id="hub-self" class="primary" ${busy ? "disabled" : ""}>${tr("Update the Hub")}</button><a href="${esc(V.hubUpdate[1])}" data-ext>${tr("or download it")}</a>`;
  $("release").textContent = cat ? `${tr("Release {v}", { v: cat.version })} · ${upd ? tr("{n} to update", { n: upd }) : tr("up to date")}` : tr("Offline");
  $("release").classList.toggle("hot", upd > 0);
  const ua = $("update-all");
  ua.hidden = !upd;
  ua.disabled = busy;
  ua.textContent = tr("Update all ({n})", { n: upd });

  $("nav-groups").innerHTML = (cat?.groups ?? [])
    .map((g) => `<button data-view="group:${g.id}">${esc(tr(g.name))} <b>${plugins().filter((p) => p.group === g.id).length}</b></button>`)
    .join("");
  document.querySelectorAll("#nav button").forEach((b) => b.classList.toggle("active", b.dataset.view === view));

  for (const id of ["plugins", "news", "settings"]) $(`view-${id}`).hidden = true;
  if (view === "news") return renderNews();
  if (view === "settings") return renderSettings();
  $("view-plugins").hidden = false;

  const group = view.startsWith("group:") ? cat?.groups.find((g) => g.id === view.slice(6)) : null;
  renderExtras();
  $("group-desc").textContent = view === "extras" ? tr("The Rack for any DAW, and the extras for REAPER. They follow your plugins: an FX chain is added once all its plugins are installed.") : group ? tr(group.desc) : view === "updates" ? tr("Plugins with a newer version in the latest release.") : view === "new" ? tr("Plugins added in release {v}.", { v: cat?.version }) : "";
  const list = shown();
  renderPick();
  // bulk install: every shown plugin that misses one of the formats picked in the bar
  const toAdd = list.filter((p) => missing(p).length);
  const nAdd = toAdd.reduce((n, p) => n + missing(p).length, 0);
  const bulk = $("bulk");
  bulk.hidden = view === "updates" || toAdd.length < 2;
  bulk.textContent = tr("Install {plugins} ({files})", { plugins: plural(toAdd.length, "plugin", "plugins"), files: plural(nAdd, "file", "files") });
  bulk.title = tr("Installs the formats picked on the left in every plugin shown that does not have them yet");
  bulk.onclick = () => run(installReq(toAdd), tr("Installed {what}", { what: plural(nAdd, "plugin file", "plugin files") }));
  bulk.disabled = busy;
  // bulk remove: every shown plugin that is installed, in all its formats
  const toRemove = list.filter((p) => isInstalled(p.id));
  const bulkRm = $("bulk-remove");
  bulkRm.hidden = toRemove.length < 2 || (view === "all" && !$("search").value.trim());
  bulkRm.textContent = tr("Remove {what}", { what: plural(toRemove.length, "plugin", "plugins") });
  bulkRm.onclick = () => confirmRemove(toRemove);
  bulkRm.disabled = busy;

  $("grid").innerHTML = list
    .map(
      (p) => `<article class="card" data-id="${p.id}" tabindex="0">
        <div class="shot">${shotHtml(p)}</div>
        <div class="card-body">
          <div class="card-name">${esc(p.name)}</div>
          <div class="card-tag">${esc(p.tagline)}</div>
          <div class="card-foot"><div class="chips">${chips(p)}</div><span class="acts">${cardButton(p)}</span></div>
        </div>
      </article>`
    )
    .join("");
  document.querySelectorAll(".card-foot button").forEach((b) => (b.disabled = busy && b.dataset.act !== "open"));
  const empty = $("empty");
  empty.hidden = list.length > 0;
  empty.textContent = !cat
    ? tr("The Hub could not reach GitHub yet. Connect to the internet and click Check for updates.")
    : tr(view === "installed" ? "No NULL JSFX plugins installed yet." : view === "updates" ? "Everything is up to date." : "No plugin matches that search.");
}

/** the format toggles in the bar: the same list as Settings > Formats */
function renderPick() {
  $("pick").innerHTML = `<span class="muted">${tr("Install as")}</span>` + FORMATS.map(([f, label, desc]) =>
    `<button class="chip-btn ${V.settings.formats.includes(f) ? "on" : ""}" data-pick="${f}" title="${esc(`${label}: ${tr(desc)}`)}" aria-pressed="${V.settings.formats.includes(f)}">${label}</button>`).join("");
}

async function togglePick(f) {
  const formats = V.settings.formats.includes(f) ? V.settings.formats.filter((x) => x !== f) : FORMATS.map(([k]) => k).filter((k) => k === f || V.settings.formats.includes(k));
  if (!formats.length) return toast(tr("Pick at least one format."), true);
  try {
    V = await invoke("hub_save_settings", { settings: { ...V.settings, formats } });
  } catch (e) {
    toast(String(e), true);
  }
  render();
}

// --- removing --------------------------------------------------------------------------------

let confirmAction = null;
function ask(title, text, okText, action, plain) {
  $("c-title").textContent = title;
  $("c-text").textContent = text;
  $("c-ok").textContent = okText;
  $("c-ok").classList.toggle("danger", !plain);
  confirmAction = action;
  $("confirm").hidden = false;
  $("c-cancel").focus();
}
function closeConfirm() { $("confirm").hidden = true; confirmAction = null; }

function confirmRemove(list) {
  const req = removeReq(list);
  const what = list.length === 1 ? list[0].name : plural(list.length, "plugin", "plugins");
  const fmts = [...new Set(req.remove.map(([, f]) => label(f)))].join(", ");
  ask(tr("Remove {what}?", { what }), `${tr("Removes {files} ({formats}).", { files: plural(req.remove.length, "file", "files"), formats: fmts })} ${tr(list.length === 1
    ? "FX chains that use it go too, and so do its factory presets you did not change. Your own presets stay. Projects that use it will show it as missing."
    : "FX chains that use them go too, and so do their factory presets you did not change. Your own presets stay. Projects that use them will show them as missing.")}`,
    tr("Remove"), () => run(req, tr("Removed {what}", { what })));
}

function confirmRemoveAll() {
  const list = plugins().filter((p) => isInstalled(p.id));
  const extras = Object.entries(V.state.extras ?? {}).filter(([, s]) => s.installed).map(([id]) => id);
  if (!list.length && !extras.length) return toast(tr("Nothing from NULL JSFX is installed."));
  const req = { ...removeReq(list), extras: [], extras_remove: extras };
  ask(tr("Remove everything?"), tr("Removes all {plugins} in every format ({files}) and the extras: FX chains, the tablet remote, the font and the factory presets you did not change. Your own presets and REAPER's settings stay. The Hub itself stays, to install them again later.",
    { plugins: plural(list.length, "NULL JSFX plugin", "NULL JSFX plugins"), files: plural(req.remove.length, "file", "files") }),
    tr("Remove everything"), () => run(req, tr("Removed everything from NULL JSFX")));
}

// --- the Hub itself, and checking the installed files ------------------------------------------

async function selfUpdate() {
  if (busy) return;
  busy = true;
  render();
  showProgress(tr("Downloading the new NULL JSFX Hub"), -1);
  try {
    const manual = await invoke("hub_self_update");
    if (manual) {
      toast(tr("The new Hub is downloaded. Open the file shown to install it."));
      T.opener.revealItemInDir(manual);
    } else {
      showProgress(tr("Starting the new NULL JSFX Hub"), -1);
      return; // this Hub quits now
    }
  } catch (e) {
    toast(`${e} ${tr("You can still download it with the link.")}`, true);
  }
  busy = false;
  $("progress").hidden = true;
  render();
}

async function verify() {
  if (busy) return;
  busy = true;
  render();
  showProgress(tr("Checking the installed files"), -1);
  let broken = null;
  try {
    broken = await invoke("hub_verify");
  } catch (e) {
    toast(String(e), true);
  }
  busy = false;
  $("progress").hidden = true;
  render();
  if (!broken) return;
  const n = Object.values(V.state.plugins).reduce((a, m) => a + Object.values(m).filter((x) => x.installed && !x.outdated).length, 0);
  if (!broken.length) return toast(n ? tr("All {what} match release {v}.", { what: plural(n, "plugin file", "plugin files"), v: V.catalog.version }) : tr("Nothing up to date to check yet."));
  const names = broken.map(([id, f]) => `${plugins().find((p) => p.id === id)?.name ?? id} ${label(f)}`);
  const what = plural(broken.length, "plugin file", "plugin files");
  ask(tr("Repair {what}?", { what }),
    tr("These differ from release {v} (damaged, changed or with files missing): {names}. Repair puts the release's files back.", { v: V.catalog.version, names: names.join(", ") }),
    tr("Repair"), () => run({ install: broken, remove: [] }, tr("Repaired {what}", { what })), true);
}

async function exportList() {
  try {
    const file = await invoke("hub_export");
    toast(tr("Saved: {file}", { file }));
    T.opener.revealItemInDir(file);
  } catch (e) {
    toast(String(e), true);
  }
}

/** a list saved by "Save the list" on another computer: install what is missing from it */
async function importList(file) {
  let sel;
  try {
    sel = JSON.parse(await file.text());
  } catch (_) { sel = null; }
  if (sel?.kind !== "null-jsfx-selection") return toast(tr("That file is not a NULL JSFX list (saved with Save the list)."), true);
  const install = [];
  for (const [id, fmts] of Object.entries(sel.plugins || {})) {
    const p = plugins().find((x) => x.id === id);
    if (!p) continue;
    for (const f of fmts) if (p.formats.includes(f) && !st(id)[f]?.installed) install.push([id, f]);
  }
  const extras = (sel.extras || []).filter((x) => V.catalog.extras.some((e) => e.id === x) && !extraSt(x).installed);
  if (!install.length && !extras.length) return toast(tr("Everything on that list is already installed."));
  const n = new Set(install.map(([id]) => id)).size;
  const names = extras.map((x) => V.catalog.extras.find((e) => e.id === x).name);
  const what = plural(n, "plugin", "plugins");
  ask(tr("Install {what} from the list?", { what }),
    tr("{files} that are not here yet{extras}, from release {v}. What is installed here already stays as it is.",
      { files: plural(install.length, "plugin file", "plugin files"), extras: names.length ? tr(", and {names}", { names: names.join(", ") }) : "", v: V.catalog.version }),
    tr("Install"), () => run({ install, remove: [], extras, extras_remove: [] }, tr("Installed {what} from the list", { what })), true);
}

function renderExtras() {
  const box = $("extras");
  box.hidden = view !== "extras" || !V.catalog;
  if (box.hidden) return;
  box.innerHTML = (V.catalog.extras || [])
    .map((x) => {
      const s = extraSt(x.id);
      const status = s.outdated ? `${tr("update")}: ${s.version ? `${esc(s.version)} → ` : ""}${esc(V.catalog.version)}` : s.installed ? `${tr("on")}${s.version ? ` · ${esc(s.version)}` : ""}` : tr("off");
      const buttons = s.installed
        ? `${s.outdated ? `<button data-extra="${x.id}" data-do="add">${tr("Update")}</button>` : ""}<button class="ghost" data-extra="${x.id}" data-do="remove">${tr("Turn off")}</button>`
        : `<button data-extra="${x.id}" data-do="add">${tr("Turn on")}</button>`;
      const steps = x.id === "remote" && s.installed ? `<button class="ghost" data-steps>${tr("Setup steps in REAPER")}</button>` : "";
      return `<div class="extra" data-id="${x.id}">
        <div class="extra-head"><span class="extra-name">${esc(tr(x.name))}</span><span class="st ${s.outdated ? "old" : ""}">${status}</span></div>
        <p>${esc(tr(x.desc))}</p>
        <div class="extra-actions">${buttons}${steps}</div>
      </div>`;
    })
    .join("");
  box.querySelectorAll("button").forEach((b) => (b.disabled = busy));
}

function inline(s) {
  return esc(s).replace(/\*\*(.+?)\*\*/g, "<b>$1</b>").replace(/`(.+?)`/g, "<code>$1</code>");
}

function renderNews() {
  $("view-news").hidden = false;
  const cat = V.catalog;
  $("news-title").textContent = cat ? `${tr("Release {v}", { v: cat.version })}${cat.date ? ` · ${cat.date}` : ""}` : tr("What's new");
  const lines = (cat?.notes || tr("No notes for this release.")).split(/\r?\n/);
  let out = "", inList = false;
  for (const l of lines) {
    const item = l.match(/^\s*[-*]\s+(.*)/);
    if (item && !inList) { out += "<ul>"; inList = true; }
    if (!item && inList) { out += "</ul>"; inList = false; }
    if (item) out += `<li>${inline(item[1])}</li>`;
    else if (/^#+\s/.test(l)) out += `<h4>${inline(l.replace(/^#+\s/, ""))}</h4>`;
    else if (l.trim()) out += `<p>${inline(l)}</p>`;
  }
  if (inList) out += "</ul>";
  $("news").innerHTML = out;
}

function renderSettings() {
  $("view-settings").hidden = false;
  const s = V.settings;
  $("set-formats").innerHTML = FORMATS.map(
    ([f, label, desc]) => `<label><input type="checkbox" value="${f}" ${s.formats.includes(f) ? "checked" : ""}> ${label} <span class="muted">(${esc(tr(desc))})</span></label>`
  ).join("");
  $("set-check").checked = s.check_updates;
  $("set-notify").checked = s.notify;
  $("set-autostart").checked = s.autostart;
  $("set-reaper").value = s.targets.reaper;
  $("set-vst3").value = s.targets.vst3;
  $("set-clap").value = s.targets.clap;
  const n = plugins().filter((p) => isInstalled(p.id)).length;
  $("verify").disabled = busy || !V.catalog;
  $("export").disabled = busy || !V.catalog;
  $("import").disabled = busy || !V.catalog;
  $("remove-all").disabled = busy || (!n && !Object.values(V.state.extras ?? {}).some((x) => x.installed));
  $("remove-all-n").textContent = n ? tr("{what} installed now.", { what: plural(n, "plugin", "plugins") }) : tr("No plugins installed now.");
  $("set-lang").value = LANG;
  $("hub-version").textContent = V.hubVersion;
  $("data-dir").textContent = V.dataDir;
  $("set-msg").textContent = "";
}

async function saveSettings() {
  const settings = {
    ...V.settings,
    formats: [...document.querySelectorAll("#set-formats input:checked")].map((i) => i.value),
    check_updates: $("set-check").checked,
    notify: $("set-notify").checked,
    autostart: $("set-autostart").checked,
    targets: { ...V.settings.targets, reaper: $("set-reaper").value.trim(), vst3: $("set-vst3").value.trim(), clap: $("set-clap").value.trim() },
  };
  if (!settings.formats.length) return ($("set-msg").textContent = tr("Pick at least one format."));
  try {
    V = await invoke("hub_save_settings", { settings });
    render();
    $("set-msg").textContent = tr("Saved.");
  } catch (e) {
    $("set-msg").textContent = String(e);
  }
}

// --- plugin page -----------------------------------------------------------------------------

// the demo: the music through the plugin and the same music dry, in step, so switching between
// them while it plays compares the two at the same moment
const demo = { wet: new Audio(), dry: new Audio(), on: "wet" };
for (const a of [demo.wet, demo.dry]) {
  a.preload = "none";
  a.loop = true;
  a.addEventListener("error", () => { if (a.src && !$("detail").hidden) { $("d-demo-note").textContent = tr("The demo needs the internet."); stopDemo(); } });
}
function stopDemo() {
  demo.wet.pause();
  demo.dry.pause();
  $("d-play").textContent = tr("Play");
  $("d-shot").querySelector("video")?.pause();
}
function setDemo(p) {
  stopDemo();
  const d = p.demo;
  $("d-demo").hidden = !d || !V.catalog.demo_dry;
  if ($("d-demo").hidden) return;
  if (demo.wet.src !== d.url) {
    demo.wet.src = d.url;
    demo.dry.src = V.catalog.demo_dry;
    demo.wet.currentTime = demo.dry.currentTime = 0;
  }
  pickDemo("wet");
  $("d-wet").textContent = p.name;
  $("d-demo-note").textContent = tr('Preset "{preset}", on a short mix', { preset: d.preset });
}
function pickDemo(which) {
  const from = demo[demo.on], to = demo[which];
  demo.on = which;
  $("d-wet").classList.toggle("on", which === "wet");
  $("d-dry").classList.toggle("on", which === "dry");
  $("d-wet").setAttribute("aria-pressed", which === "wet");
  $("d-dry").setAttribute("aria-pressed", which === "dry");
  if (from === to || from.paused) return;
  to.currentTime = from.currentTime;
  to.play().catch(() => {});
  from.pause();
}
function playDemo() {
  const a = demo[demo.on];
  if (!a.paused) return stopDemo();
  $("d-shot").querySelector("video")?.pause();
  a.play().then(() => ($("d-play").textContent = tr("Stop"))).catch(() => { $("d-demo-note").textContent = tr("The demo needs the internet."); });
}

function openDetail(id) {
  const p = plugins().find((x) => x.id === id);
  if (!p) return;
  detailId = id;
  const g = V.catalog.groups.find((x) => x.id === p.group);
  $("d-group").textContent = g ? tr(g.name) : "";
  $("d-name").textContent = p.name;
  $("d-tagline").textContent = p.tagline;
  $("d-shot").innerHTML = shotHtml(p, true);
  // the video and the demo never play over each other
  $("d-shot").querySelector("video")?.addEventListener("play", () => { demo.wet.pause(); demo.dry.pause(); $("d-play").textContent = tr("Play"); });
  $("d-guide").href = p.guide || "https://nulljsfx.tech";
  const s = st(id);
  $("d-formats").innerHTML = FORMATS.filter(([f]) => p.formats.includes(f))
    .map(([f, label, desc]) => {
      const x = s[f] || {};
      const status = x.outdated ? `${tr("update")}: ${x.version ? `${x.version} → ` : ""}${V.catalog.version}` : x.held ? tr("kept at {v}", { v: x.version }) : x.installed ? `${tr("installed")} ${x.version ?? ""}` : tr("not installed");
      return `<label><input type="checkbox" data-f="${f}" ${x.installed ? "checked" : ""} ${busy ? "disabled" : ""}> ${label}</label>
        <span class="desc">${esc(tr(desc))}</span><span class="st ${x.outdated ? "old" : ""}">${status}</span>`;
    })
    .join("");
  $("d-sliders").innerHTML = (p.sliders || []).map((x) => `<dt>${esc(x.name)} <span class="muted">${esc(x.range)}</span></dt><dd>${esc(x.desc)}</dd>`).join("");
  $("d-uses").innerHTML = (p.uses || []).map((u) => `<li>${esc(u)}</li>`).join("");
  $("d-presets-wrap").hidden = !(p.presets || []).length;
  $("d-presets").textContent = (p.presets || []).join(" · ");
  $("d-props").textContent = p.properties || "";
  if (!$("detail").hidden && detailId === id && demo.wet.src === p.demo?.url) { /* re-render after Apply: keep playing */ } else setDemo(p);
  $("d-remove").hidden = !isInstalled(id);
  $("d-remove").disabled = busy;
  // back to the release before, or forward again from there
  const held = heldFormats(id), current = currentFormats(id), prev = V.catalog.previous?.[0];
  const rb = $("d-rollback");
  rb.hidden = !held.length && !(current.length && prev);
  rb.disabled = busy;
  rb.textContent = held.length ? tr("Update to {v}", { v: V.catalog.version }) : tr("Roll back to {v}", { v: prev });
  rb.title = held.length ? tr("{formats} kept at an earlier release", { formats: held.map(label).join(", ") }) : tr("Install the {v} version of {formats} and keep it there", { v: prev, formats: current.map(label).join(", ") });
  updateApply();
  $("detail").hidden = false;
}

function detailRequest() {
  const s = st(detailId);
  const req = { install: [], remove: [] };
  document.querySelectorAll("#d-formats input").forEach((i) => {
    const f = i.dataset.f;
    const x = s[f] || {};
    if (i.checked && (!x.installed || x.outdated)) req.install.push([detailId, f]);
    if (!i.checked && x.installed) req.remove.push([detailId, f]);
  });
  return req;
}

function updateApply() {
  const r = detailRequest();
  const btn = $("d-apply");
  btn.disabled = busy || (!r.install.length && !r.remove.length);
  const s = st(detailId);
  const upd = r.install.filter(([, f]) => s[f]?.installed).length;
  const add = r.install.length - upd;
  const parts = [];
  if (add) parts.push(tr("install {n}", { n: add }));
  if (upd) parts.push(tr("update {n}", { n: upd }));
  if (r.remove.length) parts.push(tr("remove {n}", { n: r.remove.length }));
  btn.textContent = parts.length ? parts.join(", ").replace(/^./, (c) => c.toUpperCase()) : tr("Apply");
}

// --- events ----------------------------------------------------------------------------------

document.addEventListener("click", (e) => {
  const ext = e.target.closest("a[data-ext]");
  if (ext) {
    e.preventDefault();
    T.opener.openUrl(ext.href);
    return;
  }
  if (e.target.closest("#hub-self")) return selfUpdate();
  const nav = e.target.closest("#nav button[data-view]");
  if (nav) {
    view = nav.dataset.view;
    $("main").scrollTop = 0;
    render();
    return;
  }
  const act = e.target.closest(".card-foot button");
  if (act) {
    e.stopPropagation();
    const p = plugins().find((x) => x.id === act.dataset.id);
    if (act.dataset.act === "install") run(installReq([p]), tr("{name} installed", { name: p.name }));
    else if (act.dataset.act === "update") run(updateReq([p]), tr("{name} updated", { name: p.name }));
    else if (act.dataset.act === "remove") confirmRemove([p]);
    else openDetail(p.id);
    return;
  }
  const pick = e.target.closest("button[data-pick]");
  if (pick) {
    if (!busy) togglePick(pick.dataset.pick);
    return;
  }
  if (e.target.closest("button[data-steps]")) {
    const sep = V.os === "Windows" ? "\\" : "/";
    T.opener.revealItemInDir(`${V.dataDir}${sep}Remote-INSTALL.txt`);
    return;
  }
  const ex = e.target.closest("button[data-extra]");
  if (ex) {
    const x = V.catalog.extras.find((y) => y.id === ex.dataset.extra);
    const add = ex.dataset.do === "add";
    const note = add && x.id === "remote" ? ` ${tr("Finish with the setup steps in REAPER (button below).")}` : "";
    run({ install: [], remove: [], extras: add ? [x.id] : [], extras_remove: add ? [] : [x.id] }, `${tr(x.name)}: ${tr(add ? "on" : "off")}.${note}`);
    return;
  }
  const card = e.target.closest(".card");
  if (card) openDetail(card.dataset.id);
});
// a screenshot that can't load (offline) leaves the plain placeholder; the page's CSP allows no
// inline handlers, so this listens for image errors on the whole page
document.addEventListener("error", (e) => {
  if (e.target.tagName === "IMG" && e.target.parentNode) e.target.replaceWith(document.createTextNode("NULL"));
}, true);
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape" && !$("confirm").hidden) return closeConfirm();
  if (e.key === "Escape" && !$("detail").hidden) $("detail").hidden = true, (detailId = null), stopDemo();
  if (e.key === "Enter" && e.target.classList?.contains("card")) openDetail(e.target.dataset.id);
});
$("d-close").onclick = () => (($("detail").hidden = true), (detailId = null), stopDemo());
$("detail").addEventListener("click", (e) => { if (e.target.id === "detail") $("d-close").onclick(); });
$("d-formats").addEventListener("change", updateApply);
$("d-apply").onclick = () => {
  const p = plugins().find((x) => x.id === detailId);
  run(detailRequest(), tr("{name}: done", { name: p.name }));
};
$("d-play").onclick = playDemo;
$("d-wet").onclick = () => pickDemo("wet");
$("d-dry").onclick = () => pickDemo("dry");
$("d-rollback").onclick = () => {
  const p = plugins().find((x) => x.id === detailId);
  const held = heldFormats(p.id);
  if (held.length) return run({ install: held.map((f) => [p.id, f]), remove: [] }, tr("{name} is up to date again", { name: p.name }));
  const prev = V.catalog.previous[0];
  const fmts = currentFormats(p.id);
  ask(tr("Roll {name} back to {v}?", { name: p.name, v: prev }),
    tr("Installs {formats} of {name} from release {v}. It stays there, without update notices, until you click Update here. Projects keep their settings.", { formats: fmts.map(label).join(", "), name: p.name, v: prev }),
    tr("Roll back"), () => run({ install: fmts.map((f) => [p.id, f]), remove: [], from: prev }, tr("{name} is back at {v}", { name: p.name, v: prev })), true);
};
$("d-remove").onclick = () => confirmRemove([plugins().find((x) => x.id === detailId)]);
$("c-cancel").onclick = closeConfirm;
$("confirm").addEventListener("click", (e) => { if (e.target.id === "confirm") closeConfirm(); });
$("c-ok").onclick = () => { const go = confirmAction; closeConfirm(); if (go) go(); };
$("remove-all").onclick = confirmRemoveAll;
$("verify").onclick = verify;
$("export").onclick = exportList;
$("import").onclick = () => $("import-file").click();
$("import-file").addEventListener("change", (e) => {
  const f = e.target.files[0];
  e.target.value = "";
  if (f) importList(f);
});
$("search").addEventListener("input", render);
$("refresh").onclick = () => refresh(false);
$("update-all").onclick = () => {
  const list = plugins().filter((p) => outdatedFormats(p.id).length);
  run(updateReq(list, outdatedExtras()), tr("Updated to release {v}", { v: V.catalog.version }));
};
$("set-save").onclick = saveSettings;
// the language applies at once and is saved with the other settings
$("set-lang").addEventListener("change", async () => {
  setLang($("set-lang").value);
  try { V = await invoke("hub_save_settings", { settings: { ...V.settings, language: LANG } }); } catch (e) { toast(String(e), true); }
  render();
});
$("set-defaults").onclick = async () => {
  const t = await invoke("hub_default_targets");
  $("set-reaper").value = t.reaper;
  $("set-vst3").value = t.vst3;
  $("set-clap").value = t.clap;
};

(async () => {
  T.event.listen("hub-progress", (e) => onProgress(e.payload));
  // the daily check ran in the background
  T.event.listen("hub-changed", async () => {
    if (busy) return;
    V = await invoke("hub_load");
    render();
  });
  V = await invoke("hub_load");
  // the first start follows the computer's language; Settings changes it
  setLang(V.settings.language || (navigator.language || "").slice(0, 2));
  if (!V.settings.language) {
    try { V = await invoke("hub_save_settings", { settings: { ...V.settings, language: LANG } }); } catch (_) { /* asks again next time */ }
  }
  render();
  refresh(true);
})();
