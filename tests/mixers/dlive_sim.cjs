// A software Allen & Heath dLive or Avantis for the tests and for trying the Mixers tab without
// hardware. It listens on TCP (port 51325), keeps faders, mutes, names, colours, sends and mute
// groups on the console's base MIDI channel N and N+1..N+4 as the protocol documents give them,
// takes sets (faders as 7-bit NRPN, mutes as note ons, sends as System Exclusive), answers name
// and colour requests and (unless gets: false, as the Avantis document gives none) the gets, and
// sends every change to every other connected remote. A mix answers its name request only if it
// is set up (opts.mixes), as the profile assumes a real console does (Bible C24).
// It is written from the documents on its own, not from android/web/mixers/dlive/profile.js, so
// the two readings check each other; neither is checked against a real console.
//   node tests/mixers/dlive_sim.cjs [--port 51325] [--model Avantis] [--channel 11]
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const net = require('net');

const HEADER = [0xf0, 0x00, 0x00, 0x1a, 0x50, 0x10, 0x01, 0x00];
const MODELS = {
  dLive: { inputs: 128, monoAux: 62, stAux: 31, returns: 16, dca: 24, mg: 0x4e },
  Avantis: { inputs: 64, monoAux: 40, stAux: 20, returns: 12, dca: 16, mg: 0x46 },
};

function start(opts = {}) {
  const model = opts.model || 'dLive';
  const M = MODELS[model];
  const N = opts.channel == null ? 0 : opts.channel;
  const gets = opts.gets !== false;
  const mixes = new Set(opts.mixes || ['aux1', 'aux2', 'saux1']); // set up on the console
  // the console's channels: "off/ch" -> {kind: 'input'|..., name, color, fader, mute}
  const chans = new Map();
  const chan = (off, ch, name) => chans.set(off + '/' + ch, { name, color: 0, fader: 0x6b, mute: 0 });
  for (let i = 1; i <= M.inputs; i++) chan(0, i - 1, i === 1 ? 'Kick' : i === 2 ? 'Vox' : '');
  for (let a = 1; a <= M.monoAux; a++) if (mixes.has('aux' + a)) chan(2, a - 1, a === 1 ? 'Wedge' : 'Aux ' + a);
  for (let a = 1; a <= M.stAux; a++) if (mixes.has('saux' + a)) chan(2, 0x40 + a - 1, a === 1 ? 'IEM Drums' : 'St Aux ' + a);
  for (let r = 1; r <= M.returns; r++) chan(4, 0x20 + r - 1, 'Reverb ' + r);
  chan(4, 0x30, 'Main');
  for (let d = 1; d <= M.dca; d++) chan(4, 0x36 + d - 1, 'DCA ' + d);
  for (let g = 1; g <= 8; g++) chan(4, M.mg + g - 1, '');
  chans.get('0/0').color = 1; // Kick red
  const sends = new Map(); // "off/ch>dOff/dCh" -> level
  const clients = new Set();
  const received = []; // every set: {what, key, value}
  let getCount = 0;

  const sysex = (body) => Buffer.from(HEADER.concat(body, [0xf7]));
  const n = (off) => (N + off) & 15;
  const faderMsg = (off, ch, v) => Buffer.from([0xb0 | n(off), 0x63, ch, 0x62, 0x17, 0x06, v]); // running status
  const muteMsg = (off, ch, m) => Buffer.from([0x90 | n(off), ch, m ? 0x7f : 0x3f, ch, 0]);
  const sendMsg = (off, ch, dOff, dCh, v) => sysex([n(off), 0x0d, ch, n(dOff), dCh, v]);
  const write = (sock, buf) => { if (sock && !sock.destroyed) sock.write(buf); };
  const others = (from, buf) => { for (const c of clients) if (c !== from) write(c, buf); };
  const offOf = (b) => (b - N + 16) % 16;

  function reader(sock) {
    let status = 0;
    let data = [];
    let sx = null;
    const nrpn = new Map(); // channel -> {msb, lsb}
    const onSysex = (m) => {
      if (!HEADER.every((b, i) => m[i] === b)) return;
      const off = offOf(m[8]);
      if (off > 4) return;
      const c = (ch) => chans.get(off + '/' + ch);
      const op = m[9];
      if (op === 0x01 && c(m[10])) { const name = c(m[10]).name; write(sock, sysex([m[8], 0x02, m[10]].concat(Array.from(name, (x) => x.charCodeAt(0))))); }
      else if (op === 0x04 && c(m[10])) write(sock, sysex([m[8], 0x05, m[10], c(m[10]).color]));
      else if (op === 0x0d && c(m[10])) {
        const k = off + '/' + m[10] + '>' + offOf(m[11]) + '/' + m[12];
        sends.set(k, m[13]);
        received.push({ what: 'send', key: k, value: m[13] });
        others(sock, sendMsg(off, m[10], offOf(m[11]), m[12], m[13]));
      } else if (op === 0x05 && gets) {
        getCount++;
        if (m[10] === 0x0b && m[11] === 0x17 && c(m[12])) write(sock, faderMsg(off, m[12], c(m[12]).fader));
        else if (m[10] === 0x09 && c(m[11])) write(sock, muteMsg(off, m[11], c(m[11]).mute));
        else if (m[10] === 0x0f && m[11] === 0x0d && c(m[12])) {
          const dOff = offOf(m[13]);
          if (!chans.has(dOff + '/' + m[14])) return; // a mix that is not set up
          const k = off + '/' + m[12] + '>' + dOff + '/' + m[14];
          write(sock, sendMsg(off, m[12], dOff, m[14], sends.get(k) || 0));
        }
      }
    };
    return (buf) => {
      for (const x of buf) {
        if (x === 0xf0) { sx = [x]; continue; }
        if (x === 0xf7) { if (sx) { sx.push(x); onSysex(sx); } sx = null; continue; }
        if (sx) { sx.push(x); continue; }
        if (x >= 0xf8) continue;
        if (x >= 0x80) { status = x; data = []; continue; }
        data.push(x);
        if (data.length < 2) continue;
        const [a, b] = data;
        data = [];
        const off = offOf(status & 15);
        if (off > 4) continue;
        if ((status & 0xf0) === 0xb0) {
          let s = nrpn.get(status);
          if (!s) nrpn.set(status, (s = { msb: -1, lsb: -1 }));
          if (a === 0x63) s.msb = b; else if (a === 0x62) s.lsb = b;
          else if (a === 0x06 && s.lsb === 0x17) {
            const c = chans.get(off + '/' + s.msb);
            if (!c) continue;
            c.fader = b;
            received.push({ what: 'fader', key: off + '/' + s.msb, value: b });
            others(sock, faderMsg(off, s.msb, b));
          }
        } else if ((status & 0xf0) === 0x90 && b > 0) {
          const c = chans.get(off + '/' + a);
          if (!c) continue;
          c.mute = b >= 0x40 ? 1 : 0;
          received.push({ what: 'mute', key: off + '/' + a, value: c.mute });
          others(sock, muteMsg(off, a, c.mute));
        }
      }
    };
  }

  const server = net.createServer((sock) => {
    clients.add(sock);
    sock.on('data', reader(sock));
    sock.on('error', () => {});
    sock.on('close', () => clients.delete(sock));
  });
  return new Promise((resolve) => {
    server.listen(opts.port || 0, opts.host || '127.0.0.1', () => {
      resolve({
        port: server.address().port,
        chans, sends, received,
        get gets() { return getCount; },
        clients: () => clients.size,
        // a change on the console's own surface: "off/ch" and the field
        fader(k, v) { const [off, ch] = k.split('/').map(Number); chans.get(k).fader = v; others(null, faderMsg(off, ch, v)); },
        mute(k, m) { const [off, ch] = k.split('/').map(Number); chans.get(k).mute = m; others(null, muteMsg(off, ch, m)); },
        name(k, s) { const [off, ch] = k.split('/').map(Number); chans.get(k).name = s; others(null, sysex([n(off), 0x02, ch].concat(Array.from(s, (x) => x.charCodeAt(0))))); },
        send(k, v) {
          const [src, dst] = k.split('>');
          const [off, ch] = src.split('/').map(Number);
          const [dOff, dCh] = dst.split('/').map(Number);
          sends.set(k, v);
          others(null, sendMsg(off, ch, dOff, dCh, v));
        },
        drop() { for (const c of clients) c.destroy(); },
        close() { for (const c of clients) c.destroy(); server.close(); },
      });
    });
  });
}

module.exports = { start };

if (require.main === module) {
  const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
  start({ port: +arg('--port', 51325), host: '0.0.0.0', model: arg('--model', 'dLive'), channel: +arg('--channel', 0) })
    .then((s) => console.log(arg('--model', 'dLive') + ' simulator on TCP port ' + s.port));
}
