// A software Yamaha console (CL, QL, TF or DM7) speaking RCP, for the tests and for trying the
// Mixers tab without hardware. It listens on TCP (port 49280), keeps the parameters below with
// the sizes and ranges of the Companion module's parameter lists (bitfocus/companion-module-
// yamaha-rcp, MIT License), answers get, set (OK set to the sender, NOTIFY set to every other
// connected remote), devinfo productname and devicename, scpmode and devstatus, and answers
// ERROR for an address or index it does not have.
// It is written from those lists on its own, not from android/web/mixers/yamaha/profile.js, so
// the two readings check each other; neither is checked against a real console (Bible C25).
//   node tests/mixers/yamaha_sim.cjs [--port 49280] [--model TF5]
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const net = require('net');

// [address, X count, Y count, min, max, default, type]
function table(model) {
  const fam = /^TF/.test(model) ? 'TF' : /^DM7/.test(model) ? 'DM7' : 'CLQL';
  const ins = { CL5: 72, CL3: 64, CL1: 48, QL5: 64, QL1: 32, DM7: 120, 'DM7 Compact': 72 }[model] || 40;
  const t = [];
  const ch = (cat, x, mixes, pan = true) => {
    t.push([cat + '/Fader/Level', x, 1, -32768, 1000, -32768, 'int'], [cat + '/Fader/On', x, 1, 0, 1, 1, 'int'],
      [cat + '/Label/Name', x, 1, 0, 0, 'ch', 'str'], [cat + '/Label/Color', x, 1, 0, 0, 'Blue', 'str']);
    if (pan) t.push([cat + '/ToSt/Pan', x, 1, -63, 63, 0, 'int']);
    if (mixes) t.push([cat + '/ToMix/Level', x, mixes, -32768, 1000, -32768, 'int']);
  };
  const NAMES = { Mix: 'MX', St: 'ST', Mono: 'MONO', DCA: 'DCA' }; // the consoles' default names
  const bus = (cat, x, dflt) => t.push([cat + '/Fader/Level', x, 1, -32768, 1000, dflt, 'int'], [cat + '/Fader/On', x, 1, 0, 1, 1, 'int'],
    [cat + '/Label/Name', x, 1, 0, 0, NAMES[cat], 'str'], [cat + '/Label/Color', x, 1, 0, 0, 'Orange', 'str']);
  if (fam === 'CLQL') {
    ch('InCh', ins, 24); ch('StInCh', 16, 24); bus('Mix', 24, 0); bus('St', 3, -32768); bus('DCA', 16, 0);
    t.push(['MuteMaster/On', 8, 1, 0, 1, 0, 'int']);
  } else if (fam === 'TF') {
    ch('InCh', 40, 20); ch('StInCh', 4, 20); ch('FxRtnCh', 4, 20); bus('Mix', 20, 0); bus('St', 2, -32768); bus('Mono', 1, -32768); bus('DCA', 8, 0);
    t.push(['MuteMaster/On', 6, 1, 0, 1, 0, 'int']);
  } else {
    ch('InCh', ins, 48); bus('Mix', 48, 0); bus('St', 4, -32768); bus('DCA', 24, 0);
    t.push(['MuteGrpCtrl/On', 12, 1, 0, 1, 0, 'int']);
  }
  return t;
}

function start(opts = {}) {
  const model = opts.model || 'TF5';
  const defs = new Map(); // address -> {xs, ys, min, max, type}
  const values = new Map(); // "address x y" -> value
  for (const [a, xs, ys, min, max, dflt, type] of table(model)) {
    const addr = 'MIXER:Current/' + a;
    defs.set(addr, { xs, ys, min, max, type });
    for (let x = 0; x < xs; x++) for (let y = 0; y < ys; y++) {
      values.set(addr + ' ' + x + ' ' + y, type === 'str' && /Name$/.test(a) ? (dflt === 'ch' ? 'ch ' + (x + 1) : dflt + ' ' + (x + 1)) : dflt);
    }
  }
  const set = (k, v) => values.set(k, v);
  set('MIXER:Current/InCh/Label/Name 0 0', 'Kick');
  set('MIXER:Current/InCh/Label/Color 0 0', 'Red');
  set('MIXER:Current/InCh/Fader/Level 0 0', 0);
  set('MIXER:Current/InCh/ToSt/Pan 1 0', -63);
  set('MIXER:Current/Mix/Label/Name 0 0', 'Wedge');
  const clients = new Set();
  const received = []; // every set: {key, value}
  let gets = 0;

  const fmt = (k) => { const v = values.get(k); return typeof v === 'string' ? '"' + v.replace(/["\\]/g, '\\$&') + '"' : String(v); };
  const write = (sock, line) => { if (sock && !sock.destroyed) sock.write(line + '\n'); };
  const others = (from, line) => { for (const c of clients) if (c !== from) write(c, line); };

  function line(sock, text) {
    const w = text.match(/"(?:[^"\\]|\\.)*"|\S+/g) || [];
    const cmd = w[0];
    if (cmd === 'devinfo') {
      if (w[1] === 'productname') write(sock, 'OK devinfo productname "' + model + '"');
      else if (w[1] === 'devicename') write(sock, 'OK devinfo devicename "FOH ' + model + '"');
      else write(sock, 'ERROR devinfo ' + (w[1] || '') + ' UnknownAddress');
      return;
    }
    if (cmd === 'scpmode') { write(sock, 'OK scpmode ' + w.slice(1).join(' ')); return; }
    if (cmd === 'devstatus') { write(sock, 'OK devstatus runmode "normal"'); return; }
    if (cmd !== 'get' && cmd !== 'set') { write(sock, 'ERROR ' + (cmd || '') + ' UnknownCommand'); return; }
    const d = defs.get(w[1]);
    const x = +w[2];
    const y = +w[3];
    if (!d || !(x >= 0 && x < d.xs) || !(y >= 0 && y < d.ys)) { write(sock, 'ERROR ' + cmd + ' ' + w.slice(1, 4).join(' ') + ' WrongFormat'); return; }
    const k = w[1] + ' ' + x + ' ' + y;
    if (cmd === 'get') { gets++; write(sock, 'OK get ' + k + ' ' + fmt(k)); return; }
    let v = w[4];
    if (v == null) { write(sock, 'ERROR set ' + k + ' WrongFormat'); return; }
    if (d.type === 'str') v = v.replace(/^"|"$/g, '').replace(/\\(.)/g, '$1');
    else v = Math.max(d.min, Math.min(d.max, parseInt(v, 10)));
    values.set(k, v);
    received.push({ key: k, value: v });
    write(sock, 'OK set ' + k + ' ' + fmt(k));
    others(sock, 'NOTIFY set ' + k + ' ' + fmt(k));
  }

  const server = net.createServer((sock) => {
    clients.add(sock);
    let rest = '';
    sock.setEncoding('utf8');
    sock.on('data', (s) => {
      const lines = (rest + s).split('\n');
      rest = lines.pop();
      for (const l of lines) if (l.trim()) line(sock, l.trim());
    });
    sock.on('error', () => {});
    sock.on('close', () => clients.delete(sock));
  });
  return new Promise((resolve) => {
    server.listen(opts.port || 0, opts.host || '127.0.0.1', () => {
      resolve({
        port: server.address().port,
        model, values, received,
        get gets() { return gets; },
        clients: () => clients.size,
        // a change on the console's own surface: "MIXER:Current/... x y" and the value
        change(k, v) { values.set(k, v); others(null, 'NOTIFY set ' + k + ' ' + fmt(k)); },
        drop() { for (const c of clients) c.destroy(); },
        close() { for (const c of clients) c.destroy(); server.close(); },
      });
    });
  });
}

module.exports = { start };

if (require.main === module) {
  const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
  start({ port: +arg('--port', 49280), host: '0.0.0.0', model: arg('--model', 'TF5') })
    .then((s) => console.log('Yamaha ' + s.model + ' simulator (RCP) on TCP port ' + s.port));
}
