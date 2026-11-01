// NULL Remote, Mixers: UDP transport over the app's native bridge (window.NullShell, written by
// the Android shell in android/app). Packets travel as base64 strings:
//   NullShell.udpOpen(localPort) -> id, NullShell.udpSend(id, host, port, b64),
//   NullShell.udpBroadcast(id, port, b64), NullShell.udpClose(id);
// incoming packets arrive as {type:'udp', id, host, port, data} through NullShell.onUdp(cb) when
// the wrapper offers it, or as window 'message' events from the WebMessagePort otherwise.
// The tests pass their own transport with the same shape (send, broadcast, onPacket, close).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullMixerTransport = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const available = () => !!(root.NullShell && root.NullShell.udpOpen);

  async function openShellUdp(localPort = 0) {
    const shell = root.NullShell;
    const Osc = root.NullOsc;
    if (!available()) throw new Error('This page needs the NULL Remote app (no UDP bridge).');
    const id = await shell.udpOpen(localPort);
    const listeners = new Set();
    const deliver = (m) => {
      if (!m || m.type !== 'udp' || m.id !== id) return;
      const bytes = Osc.fromBase64(m.data);
      for (const cb of listeners) cb(bytes, m.host, m.port);
    };
    let unhook;
    if (typeof shell.onUdp === 'function') {
      unhook = shell.onUdp(deliver);
    } else {
      const onMessage = (e) => {
        let d = e.data;
        if (typeof d === 'string') { try { d = JSON.parse(d); } catch (_) { return; } }
        deliver(d);
      };
      root.addEventListener('message', onMessage);
      unhook = () => root.removeEventListener('message', onMessage);
    }
    return {
      send: (host, port, bytes) => shell.udpSend(id, host, port, Osc.toBase64(bytes)),
      broadcast: (port, bytes) => shell.udpBroadcast(id, port, Osc.toBase64(bytes)),
      onPacket(cb) { listeners.add(cb); return () => listeners.delete(cb); },
      close() {
        listeners.clear();
        if (typeof unhook === 'function') unhook();
        shell.udpClose && shell.udpClose(id);
      },
    };
  }

  return { available, openShellUdp };
});
