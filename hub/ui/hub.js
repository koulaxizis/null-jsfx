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
function st(id) { return V.state.plugins[id] ?? {}; }
function installedFormats(id) { return Object.entries(st(id)).filter(([, s]) => s.installed).map(([f]) => f); }
function outdatedFormats(id) { return Object.entries(st(id)).filter(([, s]) => s.outdated).map(([f]) => f); }
function isInstalled(id) { return installedFormats(id).length > 0; }
function isNew(id) { return (V.settings.new_ids || []).includes(id); }

// --- talking to Rust -----------------------------------------------------------------------

async function run(request, doneText) {
  if (busy) return;
  busy = true;
  render();
  showProgress("Starting", -1);
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
  $("refresh").textContent = "Checking";
  try {
    V = await invoke("hub_refresh");
    if (!quiet) toast(V.state.outdated ? `${V.state.outdated} updates for release ${V.catalog.version}` : `Up to date: release ${V.catalog.version}`);
  } catch (e) {
    toast(V?.catalog ? `${e} Showing what the Hub knew last time.` : String(e), true);
  } finally {
    $("refresh").disabled = false;
    $("refresh").textContent = "Check for updates";
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
    showProgress(`Downloading ${p.text}: ${mb(p.done)}${p.total ? ` of ${mb(p.total)}` : ""} MB`, p.total ? p.done / p.total : -1);
  } else if (p.stage === "unpack") {
    showProgress(`Unpacking ${p.text}`, p.total ? p.done / p.total : -1);
  } else if (p.stage === "elevate") {
    showProgress("Windows asks for permission to write to the VST3/CLAP folders", -1);
  } else {
    showProgress(p.text, -1);
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

function installReq(list) {
  return { install: list.flatMap((p) => wanted(p).map((f) => [p.id, f])), remove: [] };
}

function updateReq(list) {
  return { install: list.flatMap((p) => outdatedFormats(p.id).map((f) => [p.id, f])), remove: [] };
}

// --- rendering -------------------------------------------------------------------------------

function shown() {
  const q = $("search").value.trim().toLowerCase();
  return plugins().filter((p) => {
    if (view === "installed" && !isInstalled(p.id)) return false;
    if (view === "updates" && !outdatedFormats(p.id).length) return false;
    if (view === "new" && !isNew(p.id)) return false;
    if (view.startsWith("group:") && p.group !== view.slice(6)) return false;
    if (!q) return true;
    return q.split(/\s+/).every((w) => `${p.name} ${p.tagline} ${p.keywords}`.toLowerCase().includes(w));
  });
}

function shotHtml(p, big) {
  return p.shot
    ? `<img src="${esc(p.shot)}" alt="" ${big ? "" : 'loading="lazy"'}>`
    : "NULL";
}

function chips(p) {
  const s = st(p.id);
  return (isNew(p.id) ? '<span class="chip new" title="New in this release">NEW</span>' : "") + FORMATS.filter(([f]) => p.formats.includes(f))
    .map(([f, label]) => {
      const x = s[f] || {};
      const cls = x.outdated ? "chip old" : x.installed ? "chip on" : "chip";
      const title = x.outdated ? `${label}: update available` : x.installed ? `${label}: installed` : `${label}: not installed`;
      return `<span class="${cls}" title="${title}">${label}</span>`;
    })
    .join("");
}

function cardButton(p) {
  if (outdatedFormats(p.id).length) return `<button data-act="update" data-id="${p.id}">Update</button>`;
  if (isInstalled(p.id)) return `<button class="done" data-act="open" data-id="${p.id}">Installed</button>`;
  return `<button data-act="install" data-id="${p.id}">Install</button>`;
}

function render() {
  if (!V) return;
  const cat = V.catalog;
  const s = V.state;
  $("n-all").textContent = plugins().length || "";
  $("n-installed").textContent = Object.keys(s.plugins).filter(isInstalled).length || "";
  const upd = plugins().filter((p) => outdatedFormats(p.id).length).length;
  $("n-updates").textContent = upd || "";
  const fresh = plugins().filter((p) => isNew(p.id)).length;
  $("nav-new").hidden = !fresh;
  $("n-new").textContent = fresh || "";
  const hu = $("hub-update");
  hu.hidden = !V.hubUpdate;
  if (V.hubUpdate) hu.innerHTML = `<span>A new NULL JSFX Hub (${esc(V.hubUpdate[0])}) is out.</span><a href="${esc(V.hubUpdate[1])}" data-ext>Download it</a>`;
  $("release").textContent = cat ? (upd ? `Release ${cat.version} · ${upd} to update` : `Release ${cat.version} · up to date`) : "Offline";
  $("release").classList.toggle("hot", upd > 0);
  const ua = $("update-all");
  ua.hidden = !upd;
  ua.disabled = busy;
  ua.textContent = `Update all (${upd})`;

  $("nav-groups").innerHTML = (cat?.groups ?? [])
    .map((g) => `<button data-view="group:${g.id}">${esc(g.name)} <b>${plugins().filter((p) => p.group === g.id).length}</b></button>`)
    .join("");
  document.querySelectorAll("#nav button").forEach((b) => b.classList.toggle("active", b.dataset.view === view));

  for (const id of ["plugins", "news", "settings"]) $(`view-${id}`).hidden = true;
  if (view === "news") return renderNews();
  if (view === "settings") return renderSettings();
  $("view-plugins").hidden = false;

  const group = view.startsWith("group:") ? cat?.groups.find((g) => g.id === view.slice(6)) : null;
  $("group-desc").textContent = group ? group.desc : view === "updates" ? "Plugins with a newer version in the latest release." : view === "new" ? `Plugins added in release ${cat?.version}.` : "";
  const list = shown();
  const notInstalled = list.filter((p) => !isInstalled(p.id));
  const bulk = $("bulk");
  if (view === "updates" && list.length) {
    bulk.hidden = true;
  } else if (notInstalled.length > 1) {
    bulk.hidden = false;
    bulk.textContent = `Install all ${notInstalled.length} shown`;
    bulk.onclick = () => run(installReq(notInstalled), `Installed ${notInstalled.length} plugins`);
  } else {
    bulk.hidden = true;
  }
  bulk.disabled = busy;

  $("grid").innerHTML = list
    .map(
      (p) => `<article class="card" data-id="${p.id}" tabindex="0">
        <div class="shot">${shotHtml(p)}</div>
        <div class="card-body">
          <div class="card-name">${esc(p.name)}</div>
          <div class="card-tag">${esc(p.tagline)}</div>
          <div class="card-foot"><div class="chips">${chips(p)}</div>${cardButton(p)}</div>
        </div>
      </article>`
    )
    .join("");
  document.querySelectorAll(".card-foot button").forEach((b) => (b.disabled = busy && b.dataset.act !== "open"));
  const empty = $("empty");
  empty.hidden = list.length > 0;
  empty.textContent = !cat
    ? "The Hub could not reach GitHub yet. Connect to the internet and click Check for updates."
    : view === "installed" ? "No NULL JSFX plugins installed yet." : view === "updates" ? "Everything is up to date." : "No plugin matches that search.";
}

function inline(s) {
  return esc(s).replace(/\*\*(.+?)\*\*/g, "<b>$1</b>").replace(/`(.+?)`/g, "<code>$1</code>");
}

function renderNews() {
  $("view-news").hidden = false;
  const cat = V.catalog;
  $("news-title").textContent = cat ? `Release ${cat.version}${cat.date ? ` · ${cat.date}` : ""}` : "What's new";
  const lines = (cat?.notes || "No notes for this release.").split(/\r?\n/);
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
    ([f, label, desc]) => `<label><input type="checkbox" value="${f}" ${s.formats.includes(f) ? "checked" : ""}> ${label} <span class="muted">(${desc})</span></label>`
  ).join("");
  $("set-check").checked = s.check_updates;
  $("set-notify").checked = s.notify;
  $("set-autostart").checked = s.autostart;
  $("set-reaper").value = s.targets.reaper;
  $("set-vst3").value = s.targets.vst3;
  $("set-clap").value = s.targets.clap;
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
  if (!settings.formats.length) return ($("set-msg").textContent = "Pick at least one format.");
  try {
    V = await invoke("hub_save_settings", { settings });
    render();
    $("set-msg").textContent = "Saved.";
  } catch (e) {
    $("set-msg").textContent = String(e);
  }
}

// --- plugin page -----------------------------------------------------------------------------

function openDetail(id) {
  const p = plugins().find((x) => x.id === id);
  if (!p) return;
  detailId = id;
  const g = V.catalog.groups.find((x) => x.id === p.group);
  $("d-group").textContent = g ? g.name : "";
  $("d-name").textContent = p.name;
  $("d-tagline").textContent = p.tagline;
  $("d-shot").innerHTML = shotHtml(p, true);
  $("d-guide").href = p.guide || "https://nulljsfx.tech";
  const s = st(id);
  $("d-formats").innerHTML = FORMATS.filter(([f]) => p.formats.includes(f))
    .map(([f, label, desc]) => {
      const x = s[f] || {};
      const status = x.outdated ? `update: ${x.version ? `${x.version} → ` : ""}${V.catalog.version}` : x.installed ? `installed ${x.version ?? ""}` : "not installed";
      return `<label><input type="checkbox" data-f="${f}" ${x.installed ? "checked" : ""} ${busy ? "disabled" : ""}> ${label}</label>
        <span class="desc">${desc}</span><span class="st ${x.outdated ? "old" : ""}">${status}</span>`;
    })
    .join("");
  $("d-sliders").innerHTML = (p.sliders || []).map((x) => `<dt>${esc(x.name)} <span class="muted">${esc(x.range)}</span></dt><dd>${esc(x.desc)}</dd>`).join("");
  $("d-uses").innerHTML = (p.uses || []).map((u) => `<li>${esc(u)}</li>`).join("");
  $("d-presets-wrap").hidden = !(p.presets || []).length;
  $("d-presets").textContent = (p.presets || []).join(" · ");
  $("d-props").textContent = p.properties || "";
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
  if (add) parts.push(`install ${add}`);
  if (upd) parts.push(`update ${upd}`);
  if (r.remove.length) parts.push(`remove ${r.remove.length}`);
  btn.textContent = parts.length ? parts.join(", ").replace(/^./, (c) => c.toUpperCase()) : "Apply";
}

// --- events ----------------------------------------------------------------------------------

document.addEventListener("click", (e) => {
  const ext = e.target.closest("a[data-ext]");
  if (ext) {
    e.preventDefault();
    T.opener.openUrl(ext.href);
    return;
  }
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
    if (act.dataset.act === "install") run(installReq([p]), `${p.name} installed`);
    else if (act.dataset.act === "update") run(updateReq([p]), `${p.name} updated`);
    else openDetail(p.id);
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
  if (e.key === "Escape" && !$("detail").hidden) $("detail").hidden = true, (detailId = null);
  if (e.key === "Enter" && e.target.classList?.contains("card")) openDetail(e.target.dataset.id);
});
$("d-close").onclick = () => (($("detail").hidden = true), (detailId = null));
$("detail").addEventListener("click", (e) => { if (e.target.id === "detail") $("d-close").onclick(); });
$("d-formats").addEventListener("change", updateApply);
$("d-apply").onclick = () => {
  const p = plugins().find((x) => x.id === detailId);
  run(detailRequest(), `${p.name}: done`);
};
$("search").addEventListener("input", render);
$("refresh").onclick = () => refresh(false);
$("update-all").onclick = () => {
  const list = plugins().filter((p) => outdatedFormats(p.id).length);
  run(updateReq(list), `Updated ${list.length} plugins to release ${V.catalog.version}`);
};
$("set-save").onclick = saveSettings;
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
  render();
  refresh(true);
})();
