// A software Ardour OSC surface for the tests of the Ardour/Mixbus connector
// (android/web/daws/ardour/ardour.js). It answers the messages the connector uses with the
// replies of Ardour's libs/surfaces/osc/osc.cc, argument for argument: /set_surface,
// /strip/list ("#reply" or "/reply", "end_route_list"), /strip/plugin/list,
// /strip/plugin/descriptor (+ _end), /strip/plugin/parameter (plugin range, ignored outside it),
// activate/deactivate, the strip controls with their feedback, meters, heartbeat and transport.
// Runs alone too: node tests/daws/ardour_sim.cjs [port]
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const dgram = require('dgram');
const path = require('path');
const Osc = require('../../android/web/mixers/core/osc.js');

const INPUT = 0x80;

// a NULL plugin as Ardour shows it: one input parameter per slider (native name, no unit),
// with the slider's range (normalized: 0..1 instead, as a host may show a VST3)
function nullPlugin(id, normalized) {
  require(path.join(__dirname, '../../web/fx', id + '.js'));
  const FX = globalThis.NULLFX[id];
  const params = FX.sliders.map((s) => ({
    name: s.name.replace(/\s*\([^)]*\)\s*$/, ''),
    flags: INPUT | (s.inc >= 1 ? 2 : 0), type: 'FLOAT',
    min: normalized ? 0 : s.min, max: normalized ? 1 : s.max, fmt: '',
    points: [], value: normalized ? (s.def - s.min) / (s.max - s.min || 1) : s.def,
  }));
  // a host adds parameters of its own (VST3 bypass) and outputs (LV2 latency)
  params.push({ name: 'latency', flags: 0, type: 'FLOAT', min: 0, max: 1e6, fmt: '', points: [], value: 0 });
  return { name: FX.name, enabled: true, params };
}

function defaultSession() {
  return {
    rate: 48000,
    strips: [
      { type: 'AT', name: 'Vocal', ins: 1, outs: 2, mute: 0, solo: 0, rec: 0, fader: 0.7817, pan: 0.5,
        plugins: [nullPlugin('tilt', false), nullPlugin('delay', true),
          { name: 'ACE Reverb', enabled: true, params: [
            { name: 'Room Size', flags: INPUT, type: 'FLOAT', min: 0, max: 1, fmt: '', points: [], value: 0.5 },
            { name: 'Mode', flags: INPUT | 1 | 2, type: 'INT', min: 0, max: 2, fmt: '', points: [[0, 'Hall'], [1, 'Room'], [2, 'Plate']], value: 1 },
            { name: 'Out Level', flags: 0, type: 'FLOAT', min: 0, max: 1, fmt: '', points: [], value: 0 },
          ] }] },
      { type: 'AT', name: 'Guitar', ins: 2, outs: 2, mute: 1, solo: 0, rec: 1, fader: 0.6, pan: 0.3, plugins: [] },
      { type: 'B', name: 'Reverb Bus', ins: 2, outs: 2, mute: 0, solo: 0, fader: 0.7817, pan: 0.5, plugins: [] },
      { type: 'MA', name: 'Master', ins: 2, outs: 2, mute: 0, solo: 0, fader: 0.7817, pan: 0.5, plugins: [] },
    ],
  };
}

function start(opts = {}) {
  const sock = dgram.createSocket('udp4');
  const session = opts.session || defaultSession();
  const log = [];
  let surface = null; // {host, port, feedback}
  let beat = 0;
  const transport = { play: 0, stop: 1, rec: 0, loop: 0 };
  let timeMs = 0;

  const out = (address, args) => {
    if (!surface) return;
    sock.send(Buffer.from(Osc.encode(address, args)), surface.port, surface.host);
  };
  const i = (v) => ({ t: 'i', v });
  const f = (v) => ({ t: 'f', v });
  const s = (v) => ({ t: 's', v: String(v) });
  const fb = (bit) => surface && surface.feedback & bit;
  const strip = (ssid) => session.strips[ssid - 1];

  function feedback(ssid, what, value) {
    if (what === 'fader' && fb(2)) out('/strip/fader', [i(ssid), f(value)]);
    else if (what !== 'fader' && fb(1)) out('/strip/' + what, [i(ssid), f(value)]);
  }

  function handle(m, host, port) {
    const a = m.args.map((x) => x.v);
    log.push(m.address);
    switch (m.address) {
      case '/set_surface': {
        // auto port (0): replies go to where the request came from
        surface = { host, port: a[6] ? a[6] : port, feedback: a[2] | 0, gainmode: a[3] | 0 };
        session.strips.forEach((st, n) => {
          if (fb(1)) out('/strip/name', [i(n + 1), s(st.name)]);
          if (fb(2)) out('/strip/fader', [i(n + 1), f(st.fader)]);
          if (fb(2)) out('/strip/pan_stereo_position', [i(n + 1), f(st.pan)]);
        });
        if (fb(16)) for (const k of ['loop_toggle', 'transport_play', 'transport_stop', 'rec_enable_toggle']) out('/' + k, [f(0)]);
        return;
      }
      case '/strip/list': {
        if (!surface) surface = { host, port, feedback: 0, gainmode: 0 };
        const addr = fb(16384) ? '/reply' : '#reply';
        session.strips.forEach((st, n) => {
          const args = [s(st.type), s(st.name), i(st.ins), i(st.outs), i(st.mute), i(st.solo), i(n + 1)];
          if (st.type === 'AT' || st.type === 'MT') args.push(i(st.rec));
          out(addr, args);
        });
        out(addr, [s('end_route_list'), { t: 'h', v: session.rate }, { t: 'h', v: 48000 * 180 }, i(0)]);
        return;
      }
      case '/strip/plugin/list': {
        const st = strip(a[0]);
        if (!st) return;
        const args = [i(a[0])];
        st.plugins.forEach((p, n) => args.push(i(n + 1), s(p.name), i(p.enabled ? 1 : 0)));
        out('/strip/plugin/list', args);
        return;
      }
      case '/strip/plugin/descriptor': {
        const st = strip(a[0]);
        const p = st && st.plugins[a[1] - 1];
        if (!p) return;
        p.params.forEach((q, n) => {
          const args = [i(a[0]), i(a[1]), i(n + 1), s(q.name), i(q.flags), s(q.type), f(q.min), f(q.max), s(q.fmt), i(q.points.length)];
          for (const [v, label] of q.points) args.push(f(v), s(label));
          args.push({ t: 'd', v: q.value });
          out('/strip/plugin/descriptor', args);
        });
        out('/strip/plugin/descriptor_end', [i(a[0]), i(a[1])]);
        return;
      }
      case '/strip/plugin/parameter': {
        const st = strip(a[0]);
        const p = st && st.plugins[a[1] - 1];
        const q = p && p.params[a[2] - 1];
        if (!q || !(q.flags & INPUT)) return;
        // osc.cc: set only when lower <= val <= upper (float compare)
        const v = Math.fround(a[3]);
        if (v >= Math.fround(q.min) && v <= Math.fround(q.max)) q.value = v;
        else log.push('out of range');
        return;
      }
      case '/strip/plugin/activate':
      case '/strip/plugin/deactivate': {
        const st = strip(a[0]);
        const p = st && st.plugins[a[1] - 1];
        if (p) p.enabled = m.address.endsWith('/activate');
        return;
      }
      case '/strip/fader': case '/strip/pan_stereo_position': {
        const st = strip(a[0]);
        if (!st) return;
        const v = Math.min(1, Math.max(0, a[1]));
        if (m.address === '/strip/fader') { st.fader = v; feedback(a[0], 'fader', v); } else { st.pan = v; if (fb(2)) out('/strip/pan_stereo_position', [i(a[0]), f(v)]); }
        return;
      }
      case '/strip/mute': case '/strip/solo': case '/strip/recenable': {
        const st = strip(a[0]);
        if (!st) return;
        const key = m.address.slice(7) === 'recenable' ? 'rec' : m.address.slice(7);
        st[key] = a[1] > 0.5 ? 1 : 0;
        feedback(a[0], m.address.slice(7), st[key]);
        return;
      }
      case '/transport_play': transport.play = 1; transport.stop = 0; break;
      case '/transport_stop': transport.play = 0; transport.stop = 1; break;
      case '/rec_enable_toggle': transport.rec = transport.rec ? 0 : 1; break;
      case '/loop_toggle': transport.loop = transport.loop ? 0 : 1; break;
      case '/goto_start': timeMs = 0; return;
      default: return;
    }
    if (fb(16)) {
      out('/transport_play', [f(transport.play)]);
      out('/transport_stop', [f(transport.stop)]);
      out('/rec_enable_toggle', [f(transport.rec)]);
      out('/loop_toggle', [f(transport.loop)]);
    }
  }

  sock.on('message', (buf, r) => {
    let msgs;
    try { msgs = Osc.decode(new Uint8Array(buf)); } catch (e) { return; }
    for (const m of msgs) handle(m, r.address, r.port);
  });

  // heartbeat (bit 8) toggles about once a second, meters (bit 128) and the clock (bit 2048)
  const timer = setInterval(() => {
    if (!surface) return;
    beat++;
    if (fb(8) && beat % 10 === 0) out('/heartbeat', [f((beat / 10) % 2)]);
    if (transport.play) timeMs += 100;
    if (fb(2048)) {
      const t = timeMs / 1000;
      out('/position/time', [s(`${String(Math.floor(t / 3600)).padStart(2, '0')}:${String(Math.floor(t / 60) % 60).padStart(2, '0')}:${(t % 60).toFixed(3).padStart(6, '0')}`)]);
    }
    if (fb(128)) {
      session.strips.forEach((st, n) => {
        const db = st.mute ? -193 : -18 + 6 * Math.sin(beat / 3 + n);
        out('/strip/meter', [i(n + 1), f(surface.gainmode ? (db + 94) / 100 : db)]);
      });
    }
  }, 100);

  return new Promise((resolve) => sock.bind(opts.port || 0, '127.0.0.1', () => {
    resolve({
      port: sock.address().port,
      session, log, transport,
      get surface() { return surface; },
      close() { clearInterval(timer); sock.close(); },
    });
  }));
}

module.exports = { start, defaultSession, nullPlugin, INPUT };

if (require.main === module) {
  start({ port: parseInt(process.argv[2], 10) || 3819 }).then((sim) => console.log(`Ardour OSC simulator on UDP ${sim.port}`));
}
