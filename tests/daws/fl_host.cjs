// A software FL Studio for the tests of the FL Studio tab: fl_sim.py runs the real MIDI script
// (remote/flstudio/NULL JSFX/device_NULLJSFX.py) in Python against stand-ins for FL Studio's
// modules; this file connects the script's MIDI port to a network MIDI session
// (rtpmidi_peer.cjs), as rtpMIDI or macOS's network MIDI driver would, and calls OnIdle every
// 20 ms as FL Studio does. Whether FL Studio behaves the same is Bible C20.
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const { spawn } = require('child_process');
const path = require('path');
const readline = require('readline');
const Peer = require('./rtpmidi_peer.cjs');

const SIM = path.join(__dirname, 'fl_sim.py');
const SCRIPT = path.join(__dirname, '../../remote/flstudio/NULL JSFX/device_NULLJSFX.py');
const PYTHON = process.env.PYTHON || 'python3';

// a NULL plugin as FL Studio lists a VST3's parameters (one per slider, normalized 0..1)
function nullPlugin(id) {
  require(path.join(__dirname, '../../web/fx', id + '.js'));
  const FX = globalThis.NULLFX[id];
  return { name: 'NULL ' + FX.name, vst: true, params: FX.sliders.map((s) => ({
    name: s.name.replace(/\s*\([^)]*\)\s*$/, ''), v: (s.def - s.min) / (s.max - s.min || 1),
  })) };
}

// FL's mixer: the master, then inserts; a track nobody named or used is not in the window
function defaultProject(n = 3) {
  const tracks = [
    { name: 'Master', vol: 0.8, pan: 0, color: 0x404040, peak: 0.5 },
    { name: 'Vocal', vol: 0.8, pan: 0, color: 0x334dcc, peak: 0.4, // BGR: #cc4d33
      fx: [nullPlugin('delay'), null, { name: 'Fruity Limiter', params: [{ name: 'Gain', v: 0.5 }] }] },
    { name: 'Μπάσο', vol: 0.5, pan: -0.4, mute: 1, arm: 1, color: 0xcc8033 },
  ];
  for (let i = tracks.length; i < n; i++) tracks.push({ name: 'Track ' + i });
  return { tracks, transport: { play: 0, rec: 0, pos: 0 } };
}

async function start(opts = {}) {
  const project = opts.project || defaultProject(opts.tracks);
  const peer = await Peer.start();
  const args = [SIM, SCRIPT].concat(opts.noJson ? ['--no-json'] : []);
  const py = spawn(PYTHON, args, { stdio: ['pipe', 'pipe', 'inherit'] });
  const sent = [];
  const unhandled = [];
  const waiting = new Map();
  let nextId = 1;
  let closed = false;
  const cmd = (o) => { if (!closed) py.stdin.write(JSON.stringify(o) + '\n'); };
  readline.createInterface({ input: py.stdout }).on('line', (line) => {
    const m = JSON.parse(line);
    if (m.sysex) {
      const b = Array.from(Buffer.from(m.sysex, 'hex'));
      sent.push(b);
      peer.send([b]);
    } else if (m.unhandled) unhandled.push(m.unhandled);
    else if (m.id && waiting.has(m.id)) { waiting.get(m.id)(m.project); waiting.delete(m.id); }
  });
  const exited = new Promise((resolve) => py.on('exit', resolve));

  cmd({ op: 'project', p: project });
  cmd({ op: 'init' });
  peer.onMidi((m) => { if (m[0] === 0xf0) cmd({ op: 'sysex', hex: Buffer.from(m).toString('hex') }); });
  const idle = setInterval(() => cmd({ op: 'idle' }), 20);

  return {
    peer, port: peer.port, sent, unhandled,
    // the project as FL Studio has it now
    project() { const id = nextId++; return new Promise((resolve) => { waiting.set(id, resolve); cmd({ op: 'get', id }); }); },
    // a change made in FL Studio itself: set(['tracks', 2, 'name'], 'Gtr')
    set(p, v) { cmd({ op: 'set', path: p, v }); },
    async close() {
      clearInterval(idle);
      cmd({ op: 'quit' });
      closed = true;
      py.stdin.end();
      const code = await exited;
      peer.close();
      if (code) throw new Error('fl_sim.py exited with ' + code);
    },
  };
}

module.exports = { start, defaultProject, nullPlugin };
