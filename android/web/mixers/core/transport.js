// NULL Remote, Mixers: UDP transport over the app's native bridge (window.NullShell, written by
// the Android shell in android/app). Packets travel as base64 strings:
//   NullShell.udpOpen(localPort) -> id, NullShell.udpSend(id, host, port, b64),
//   NullShell.udpBroadcast(id, port, b64), NullShell.udpClose(id);
// incoming packets arrive as {type:'udp', id, host, port, data} through NullShell.on('udp', fn)
// (android/web/core/shell.js). udpOpen returns -1 when the socket cannot be opened.
// The tests pass their own transport with the same shape (send, broadcast, onPacket, close).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullMixerTransport = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const available = () => !!(root.NullShell && root.NullShell.udpOpen && root.NullShell.native !== false);

  async function openShellUdp(localPort = 0) {
    const shell = root.NullShell;
    const Osc = root.NullOsc;
    if (!available()) throw new Error('This page needs the Remote for Mixers app (no UDP bridge).');
    const id = await shell.udpOpen(localPort);
    if (id == null || id < 0) throw new Error('Cannot open a network socket on this device.');
    const listeners = new Set();
    const deliver = (m) => {
      if (!m || m.type !== 'udp' || m.id !== id) return;
      const bytes = Osc.fromBase64(m.data);
      for (const cb of listeners) cb(bytes, m.host, m.port);
    };
    shell.on('udp', deliver);
    const unhook = () => shell.off && shell.off('udp', deliver);
    return {
      send: (host, port, bytes) => shell.udpSend(id, host, port, Osc.toBase64(bytes)),
      broadcast: (port, bytes) => shell.udpBroadcast(id, port, Osc.toBase64(bytes)),
      onPacket(cb) { listeners.add(cb); return () => listeners.delete(cb); },
      close() {
        listeners.clear();
        unhook();
        shell.udpClose && shell.udpClose(id);
      },
    };
  }

  // A TCP connection through the bridge, for a console that speaks over TCP (core/nrpn.js):
  // shellTcp(host, port)(cb {onOpen, onData, onClose}) -> {send(bytes), close()}.
  //   NullShell.tcpOpen(host, port, ms) -> id, then {type: 'tcpopen' | 'tcp' | 'tcpclose', id}.
  function shellTcp(host, port) {
    const shell = root.NullShell;
    const Osc = root.NullOsc;
    if (!shell || !shell.tcpOpen || shell.native === false) throw new Error('This page needs the Remote for Mixers app (no TCP bridge).');
    return function dial(cb) {
      const id = shell.tcpOpen(host, port, 4000);
      const on = {
        tcpopen: (m) => { if (m.id === id) cb.onOpen(); },
        tcp: (m) => { if (m.id === id) cb.onData(Osc.fromBase64(m.data)); },
        tcpclose: (m) => { if (m.id === id) { off(); cb.onClose(m.error || ''); } },
      };
      const off = () => { for (const k in on) shell.off && shell.off(k, on[k]); };
      for (const k in on) shell.on(k, on[k]);
      if (id == null || id === -1) setTimeout(() => { off(); cb.onClose('cannot open a connection'); }, 0);
      return {
        send: (bytes) => shell.tcpSend(id, Osc.toBase64(bytes)),
        close: () => { off(); shell.tcpClose(id); },
      };
    };
  }

  return { available, openShellUdp, shellTcp };
});
