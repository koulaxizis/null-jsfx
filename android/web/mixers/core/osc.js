// NULL Remote, Mixers: OSC 1.0 codec shared by every mixer profile (X AIR now, X32/M32 later).
// Encodes and decodes messages and bundles as Uint8Array, with the argument types the
// Behringer/Midas consoles use: i (int32), f (float32), s (string), b (blob); also d (float64)
// and h (int64), which Ardour sends (t, a time tag, is decoded as a number). Big-endian, as the
// OSC spec says. Runs in the WebView and in Node (tests and the simulator).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullOsc = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const utf8enc = new TextEncoder();
  const utf8dec = new TextDecoder();
  const pad4 = (n) => (n + 3) & ~3;

  function stringBytes(s) {
    const raw = utf8enc.encode(s);
    const out = new Uint8Array(pad4(raw.length + 1)); // at least one NUL, then pad to 4
    out.set(raw);
    return out;
  }

  // Guess a type for a plain JS value: integers stay i only when passed as {t:'i'}; numbers are f.
  function normArg(a) {
    if (a && typeof a === 'object' && 't' in a) return a;
    if (typeof a === 'number') return { t: 'f', v: a };
    if (typeof a === 'string') return { t: 's', v: a };
    if (a instanceof Uint8Array) return { t: 'b', v: a };
    throw new TypeError('unsupported OSC argument: ' + a);
  }

  // encode('/ch/01/mix/fader', [0.75]) or encode('/ch/01/mix/on', [{t:'i', v:1}])
  function encode(address, args) {
    args = (args || []).map(normArg);
    const parts = [stringBytes(address), stringBytes(',' + args.map((a) => a.t).join(''))];
    for (const a of args) {
      if (a.t === 'i' || a.t === 'f') {
        const b = new Uint8Array(4);
        const dv = new DataView(b.buffer);
        if (a.t === 'i') dv.setInt32(0, a.v | 0, false);
        else dv.setFloat32(0, a.v, false);
        parts.push(b);
      } else if (a.t === 'd' || a.t === 'h') {
        const b = new Uint8Array(8);
        if (a.t === 'd') new DataView(b.buffer).setFloat64(0, a.v, false);
        else new DataView(b.buffer).setBigInt64(0, BigInt(Math.trunc(a.v)), false);
        parts.push(b);
      } else if (a.t === 's') {
        parts.push(stringBytes(String(a.v)));
      } else if (a.t === 'b') {
        const b = new Uint8Array(4 + pad4(a.v.length));
        new DataView(b.buffer).setInt32(0, a.v.length, false);
        b.set(a.v, 4);
        parts.push(b);
      } else {
        throw new TypeError('unsupported OSC type tag: ' + a.t);
      }
    }
    return concat(parts);
  }

  function concat(parts) {
    const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }

  function readString(bytes, pos) {
    let end = pos;
    while (end < bytes.length && bytes[end] !== 0) end++;
    if (end >= bytes.length) throw new RangeError('OSC string without terminator');
    return [utf8dec.decode(bytes.subarray(pos, end)), pad4(end + 1)];
  }

  // decode(bytes) -> [{address, args:[{t, v}]}...]; a bundle is flattened into its messages.
  function decode(bytes) {
    if (!(bytes instanceof Uint8Array)) bytes = new Uint8Array(bytes);
    const out = [];
    decodeInto(bytes, out);
    return out;
  }

  function decodeInto(bytes, out) {
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    let [address, pos] = readString(bytes, 0);
    if (address === '#bundle') {
      pos += 8; // time tag, ignored: consoles send everything "now"
      while (pos + 4 <= bytes.length) {
        const n = dv.getInt32(pos, false);
        pos += 4;
        if (n < 0 || pos + n > bytes.length) throw new RangeError('bad OSC bundle element size');
        decodeInto(bytes.subarray(pos, pos + n), out);
        pos += n;
      }
      return;
    }
    const args = [];
    if (pos < bytes.length) {
      let tags;
      [tags, pos] = readString(bytes, pos);
      if (tags[0] !== ',') throw new RangeError('OSC type tags must start with a comma');
      for (const t of tags.slice(1)) {
        if (t === 'i') { args.push({ t, v: dv.getInt32(pos, false) }); pos += 4; }
        else if (t === 'f') { args.push({ t, v: dv.getFloat32(pos, false) }); pos += 4; }
        // 64-bit types (Ardour sends d for plugin values and h for the session's sample rate)
        else if (t === 'd') { args.push({ t, v: dv.getFloat64(pos, false) }); pos += 8; }
        else if (t === 'h' || t === 't') { args.push({ t, v: Number(dv.getBigInt64(pos, false)) }); pos += 8; }
        else if (t === 's') { let s; [s, pos] = readString(bytes, pos); args.push({ t, v: s }); }
        else if (t === 'b') {
          const n = dv.getInt32(pos, false);
          pos += 4;
          if (n < 0 || pos + n > bytes.length) throw new RangeError('bad OSC blob size');
          args.push({ t, v: bytes.slice(pos, pos + n) });
          pos += pad4(n);
        } else if (t === 'T' || t === 'F' || t === 'N') {
          args.push({ t, v: t === 'T' ? true : t === 'F' ? false : null });
        } else {
          throw new RangeError('unsupported OSC type tag: ' + t);
        }
      }
    }
    out.push({ address, args });
  }

  // base64 helpers for the native UDP bridge, which carries packets as base64 strings
  function toBase64(bytes) {
    if (typeof Buffer !== 'undefined') return Buffer.from(bytes).toString('base64');
    let s = '';
    for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    return btoa(s);
  }
  function fromBase64(b64) {
    if (typeof Buffer !== 'undefined') return new Uint8Array(Buffer.from(b64, 'base64'));
    const s = atob(b64);
    const out = new Uint8Array(s.length);
    for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
    return out;
  }

  return { encode, decode, toBase64, fromBase64 };
});
