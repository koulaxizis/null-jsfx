// A network MIDI session on the computer's side, for the tests: what macOS's MIDI Network Setup
// or rtpMIDI on Windows does when the phone invites it (../../android/web/daws/core/rtpmidi.js).
// It answers invitations on its control port and the next one, the clock sync, BY, and turns
// RTP-MIDI packets into MIDI messages; send() goes back to the phone. Options: refuse (answer
// NO), silent (answer nothing).
//   start({port}) -> {port, onMidi(cb), send(messages), bye(), mute(), close(), invited, syncs}
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const dgram = require('dgram');
const path = require('path');
const R = require(path.resolve(__dirname, '../../android/web/daws/core/rtpmidi.js'));

const IN = 0x494e, OK = 0x4f4b, NO = 0x4e4f, BY = 0x4259, CK = 0x434b;

async function bind(port) {
  const s = dgram.createSocket('udp4');
  await new Promise((resolve, reject) => { s.once('error', reject); s.bind(port, '127.0.0.1', resolve); });
  return s;
}

async function start(opts = {}) {
  let ctl, data;
  for (let i = 0; ; i++) { // two neighbouring ports
    const p = opts.port || 2 * (21000 + Math.floor(Math.random() * 9000));
    try { ctl = await bind(p); data = await bind(p + 1); break; } catch (e) {
      if (ctl) ctl.close(); ctl = null;
      if (opts.port || i > 20) throw e;
    }
  }
  const port = ctl.address().port;
  const ssrc = 0x5eed1234;
  const t0 = Date.now();
  const now = () => (Date.now() - t0) * 10;
  const listeners = new Set();
  const state = {};
  let phone = null; // {host, ctl, data, ssrc}
  let muted = !!opts.silent;
  let seq = 1;
  const peer = { port, invited: 0, syncs: 0, received: [], name: opts.name || 'Studio PC' };

  const reply = (sock, rinfo, command, token) => sock.send(R.control(command, token, ssrc, peer.name), rinfo.port, rinfo.address);
  function onControl(sock, which, b, rinfo) {
    const m = R.parseControl(b);
    if (!m || muted) return;
    if (m.command === IN) {
      peer.invited++;
      if (opts.refuse) { reply(sock, rinfo, NO, m.token); return; }
      if (which === 'ctl') phone = { host: rinfo.address, ctl: rinfo.port, data: 0, ssrc: m.ssrc, name: m.name };
      else if (phone) phone.data = rinfo.port;
      reply(sock, rinfo, OK, m.token);
    } else if (m.command === CK && which === 'data') {
      peer.syncs++;
      if (m.count === 0) data.send(R.clock(ssrc, 1, [m.t[0], now(), 0]), rinfo.port, rinfo.address);
    } else if (m.command === BY) {
      phone = null;
      peer.byes = (peer.byes || 0) + 1;
    }
  }
  ctl.on('message', (b, r) => onControl(ctl, 'ctl', new Uint8Array(b), r));
  data.on('message', (b, r) => {
    const u = new Uint8Array(b);
    if (u[0] === 0xff && u[1] === 0xff) { onControl(data, 'data', u, r); return; }
    if (muted) return;
    R.parseRtp(u, state, (m) => { peer.received.push(m); for (const cb of listeners) cb(m); });
  });

  Object.defineProperty(peer, 'phone', { get: () => phone });
  return Object.assign(peer, {
    onMidi(cb) { listeners.add(cb); return () => listeners.delete(cb); },
    // MIDI to the phone; a long System Exclusive message goes in segments, as rtpMIDI does
    send(messages, segment = 1000) {
      if (!phone || !phone.data) return;
      const out = (msgs) => data.send(R.rtp(seq++, now(), ssrc, msgs), phone.data, phone.host);
      for (const m of messages) {
        if (m[0] !== 0xf0 || m.length <= segment) { out([m]); continue; }
        const body = m.slice(1, -1);
        for (let i = 0; i < body.length; i += segment) {
          const last = i + segment >= body.length;
          out([[i ? 0xf7 : 0xf0].concat(body.slice(i, i + segment), [last ? 0xf7 : 0xf0])]);
        }
      }
    },
    bye() { if (phone) ctl.send(R.control(BY, 0, ssrc), phone.ctl, phone.host); phone = null; },
    mute(on = true) { muted = on; },
    close() { try { ctl.close(); data.close(); } catch (e) { /* closed */ } },
  });
}

// UDP sockets shaped like the app's (rtpmidi.js shellUdp), for the phone's side in the tests
const phoneSockets = new Set();
function nodeUdp(port) {
  return new Promise((resolve) => {
    const s = dgram.createSocket('udp4');
    s.once('error', () => resolve(null));
    s.bind(port, '127.0.0.1', () => {
      phoneSockets.add(s);
      const listeners = new Set();
      s.on('message', (b, r) => { for (const cb of listeners) cb(new Uint8Array(b), r.address, r.port); });
      resolve({
        send: (host, p, bytes) => s.send(bytes, p, host),
        onPacket(cb) { listeners.add(cb); return () => listeners.delete(cb); },
        close() { phoneSockets.delete(s); try { s.close(); } catch (e) { /* closed */ } },
      });
    });
  });
}
nodeUdp.closeAll = () => { for (const s of phoneSockets) try { s.close(); } catch (e) { /* closed */ } };

module.exports = { start, nodeUdp };

if (require.main === module) {
  start({ port: +(process.argv[2] || 5004) }).then((p) => {
    console.log('network MIDI session on UDP ' + p.port + ' and ' + (p.port + 1));
    p.onMidi((m) => console.log(m.length > 40 ? m.length + ' bytes ' + (R.fromSysex(m) || '') : m.map((x) => x.toString(16)).join(' ')));
  });
}
