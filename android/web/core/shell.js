// NULL JSFX Android remotes: window.NullShell, what the pages can ask of the phone.
// NULL JSFX • https://nulljsfx.tech • MIT License (the version is the app's: NullShell.info())
//
// In the Android app it wraps window.NullShellNative (android/app/.../Shell.kt). In a desktop
// browser (the tests, or opening android/web by hand) it falls back to a stub: REAPER requests
// go to the page's own origin, UDP is not available, and ?app=mixers picks the mixer app.
//
//   NullShell.native                     true inside the app
//   NullShell.info()                     {app: 'reaper' | 'mixers', version, code, flavor, updateCheck, sdk}
//   NullShell.reaperSet({host, port, user, pass})   where /_/ requests go
//   NullShell.scan(port) -> Promise<{hosts: [{host, auth}], self}>
//   NullShell.udpOpen(localPort) -> id (-1 = failed)
//   NullShell.udpSend(id, host, port, base64) / udpBroadcast(id, port, base64) / udpClose(id)
//   NullShell.tcpOpen(host, port, timeoutMs) -> id; then {type: "tcpopen", id},
//     {type: "tcp", id, data: base64} per chunk read, {type: "tcpclose", id, error}
//   NullShell.tcpSend(id, base64) / tcpClose(id)
//   NullShell.on(type, fn) / off(type, fn)   messages from the phone, e.g. "udp":
//                                             {type: "udp", id, host, port, data: base64}
//   NullShell.keepAwake(bool), fullscreen(bool), haptic(), openUrl(https url)
'use strict';
(function () {
  const N = window.NullShellNative || null;
  const handlers = new Map();
  const pending = new Map();
  let nextId = 1;

  function recv(json) {
    let m;
    try { m = typeof json === 'string' ? JSON.parse(json) : json; } catch (e) { return; }
    if (!m || !m.type) return;
    if (m.type === 'scan' && pending.has(m.id)) {
      pending.get(m.id)(m);
      pending.delete(m.id);
    }
    for (const fn of handlers.get(m.type) || []) {
      try { fn(m); } catch (e) { console.error(e); }
    }
  }

  // the message channel the app opens on every page load (MainActivity.openPort)
  window.addEventListener('message', e => {
    if (e.data === 'nullshell' && e.ports && e.ports[0]) e.ports[0].onmessage = m => recv(m.data);
  });

  const stub = {
    info: () => JSON.stringify({
      app: new URLSearchParams(location.search).get('app') === 'mixers' ? 'mixers' : 'reaper',
      version: 'dev', code: 0, flavor: 'browser', updateCheck: false, sdk: 0,
    }),
    reaperSet() {},
    scan(id) { setTimeout(() => recv({ type: 'scan', id, hosts: window.__NULL_STUB_SCAN || [], self: '' }), 50); },
    udpOpen: () => -1, udpSend: () => false, udpBroadcast: () => false, udpClose() {},
    tcpOpen(host, port, ms, id) { setTimeout(() => recv({ type: 'tcpclose', id, error: 'no TCP in a browser' }), 0); },
    tcpSend: () => false, tcpClose() {},
    keepAwake() {}, fullscreen() {}, haptic() {},
    openUrl(u) { if (/^https:\/\//.test(u)) window.open(u, '_blank', 'noopener'); },
  };
  const n = N || stub;

  window.NullShell = {
    native: !!N,
    info() { try { return JSON.parse(n.info()); } catch (e) { return {}; } },
    reaperSet(c) { n.reaperSet(JSON.stringify(c || {})); },
    scan(port) {
      const id = nextId++;
      return new Promise(resolve => { pending.set(id, resolve); n.scan(id, port | 0); });
    },
    udpOpen: localPort => n.udpOpen(localPort | 0),
    udpSend: (id, host, port, b64) => n.udpSend(id, String(host), port | 0, String(b64)),
    udpBroadcast: (id, port, b64) => n.udpBroadcast(id, port | 0, String(b64)),
    udpClose: id => n.udpClose(id),
    tcpOpen(host, port, timeoutMs) {
      if (N) return N.tcpOpen(String(host), port | 0, timeoutMs | 0 || 5000);
      const id = -(nextId++);
      stub.tcpOpen(host, port, timeoutMs, id);
      return id;
    },
    tcpSend: (id, b64) => n.tcpSend(id, String(b64)),
    tcpClose: id => n.tcpClose(id),
    on(type, fn) { if (!handlers.has(type)) handlers.set(type, new Set()); handlers.get(type).add(fn); },
    off(type, fn) { handlers.get(type)?.delete(fn); },
    keepAwake: on => n.keepAwake(!!on),
    fullscreen: on => n.fullscreen(!!on),
    haptic: () => n.haptic(),
    openUrl: u => n.openUrl(String(u)),
    _recv: recv, // the fallback path when the phone's WebView has no message channels
  };
})();
