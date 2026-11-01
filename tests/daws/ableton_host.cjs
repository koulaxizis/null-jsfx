// The software Ableton Live (ableton_live.py, the real control surface remote/ableton/NullJSFX
// in a stand-in for Live's Python API) as the Node tests use it: start() builds the document,
// with the NULL plugins as Live lists a VST3 (one parameter per slider, 0..1, named like the
// slider), and returns { port, get(), set(path, value), call(path), close() }.
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const path = require('path');
const { spawn, spawnSync } = require('child_process');

const LIVE = path.join(__dirname, 'ableton_live.py');
const UNITY = 0.85; // Live's volume position for 0 dB
const PYTHON = process.env.PYTHON || 'python3';

function nullPlugin(id) {
  require(path.join(__dirname, '../../web/fx', id + '.js'));
  const FX = globalThis.NULLFX[id];
  return {
    name: 'NULL ' + FX.name, class_name: 'PluginDevice', on: true,
    params: FX.sliders.map((s) => ({
      name: s.name.replace(/\s*\([^)]*\)\s*$/, ''),
      value: (s.def - s.min) / (s.max - s.min || 1),
      shown: [s.min, s.max],
    })),
  };
}

function defaultDocument() {
  return {
    tracks: [
      { name: 'Vocal', kind: 'audio', vol: UNITY, pan: 0, color: 0xcc4d33, sends: [0.25],
        devices: [nullPlugin('tilt'), nullPlugin('delay'),
          { name: 'EQ Eight', class_name: 'Eq8', on: true, params: [
            { name: '1 Gain A', value: 0, min: -15, max: 15 },
            { name: '1 Frequency A', value: 0.25, min: 0, max: 1 },
          ] }] },
      { name: 'Synth', kind: 'midi', vol: 0.6, pan: -0.4, mute: true, arm: true, color: 0x3380cc, devices: [] },
    ],
    returns: [{ name: 'FX 1', vol: UNITY, pan: 0, color: 0x808080, devices: [] }],
    master: { name: 'Master', vol: UNITY, pan: 0, color: 0x4d4d4d, devices: [] },
  };
}

/** The script's Python must stay valid for Live 11 (Python 3.7). Returns '' or the error. */
function syntaxCheck() {
  const code = [
    'import ast, glob, os, sys',
    "for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.py'))):",
    "    ast.parse(open(f, encoding='utf-8').read(), f, feature_version=(3, 7))",
  ].join('\n');
  const r = spawnSync(PYTHON, ['-B', '-c', code, path.join(__dirname, '../../remote/ableton/NullJSFX')], { encoding: 'utf8' });
  return r.status === 0 ? '' : r.stderr || 'exit ' + r.status;
}

async function start(opts = {}) {
  const doc = opts.doc || defaultDocument();
  const py = spawn(PYTHON, ['-B', LIVE, String(opts.port || 0)], { stdio: ['pipe', 'pipe', 'pipe'] });
  const log = [];
  py.stderr.on('data', (d) => log.push(String(d)));
  let buf = '';
  let next = 1;
  const waiting = new Map();
  let ready;
  const port = new Promise((resolve, reject) => {
    ready = resolve;
    py.on('exit', (code) => reject(new Error('the software Live stopped (' + code + '): ' + log.join(''))));
  });
  py.stdout.on('data', (d) => {
    buf += d;
    let nl;
    while ((nl = buf.indexOf('\n')) >= 0) {
      const m = JSON.parse(buf.slice(0, nl));
      buf = buf.slice(nl + 1);
      if ('port' in m) { ready(m.port); continue; }
      const w = waiting.get(m.id);
      waiting.delete(m.id);
      if (w) (m.error ? w.reject(new Error(m.error)) : w.resolve(m.v));
    }
  });
  py.stdin.write(JSON.stringify(doc) + '\n');
  const ask = (c) => new Promise((resolve, reject) => {
    const id = next++;
    waiting.set(id, { resolve, reject });
    py.stdin.write(JSON.stringify({ id, ...c }) + '\n');
  });
  return {
    port: await port,
    log,
    get: () => ask({ op: 'get' }),
    set: (p, value) => ask({ op: 'set', path: p, value }),
    call: (p) => ask({ op: 'call', path: p }),
    remove: (p) => ask({ op: 'remove', path: p }),
    close() { try { py.stdin.end(); } catch (e) { /* gone */ } py.kill(); },
  };
}

module.exports = { start, defaultDocument, syntaxCheck, UNITY };

if (require.main === module) {
  start({ port: parseInt(process.argv[2], 10) || 8819 }).then((l) => console.log(`software Live with the NULL JSFX surface on TCP ${l.port}`));
}
