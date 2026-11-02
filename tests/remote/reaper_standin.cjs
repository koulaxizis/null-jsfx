// A stand-in for REAPER for the tests of the REAPER remotes: mock_reaper.lua (a project with
// three tracks, the ReaScript calls the bridge uses and the web interface commands) running the
// real bridge script (remote/Scripts/null_jsfx_remote_bridge.lua) under fengari, with NULL Delay
// on the Vocals track. It is the same set-up android_test.cjs had inline; device_test.cjs
// shares it and serves it over HTTP to an Android emulator.
//   const reaper = standin();      reaper.web('TRANSPORT') -> REAPER's answer
//   reaper.q('tracks[1].vol')      a Lua expression as a string; reaper.num(...) as a number
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const fs = require('fs');
const path = require('path');
const { lua, lauxlib, lualib, to_luastring } = require('fengari');

const root = path.resolve(__dirname, '../..');
const PAGE = path.join(root, 'remote/reaper_www_root/null_jsfx.html');
const BRIDGE = path.join(root, 'remote/Scripts/null_jsfx_remote_bridge.lua');

const luaVal = (v) => (v === null || v === undefined ? 'nil'
  : Array.isArray(v) ? '{' + v.map(luaVal).join(',') + '}'
  : typeof v === 'string' ? JSON.stringify(v) : String(v));

/** The NULL plugin list the REAPER page carries ({s, n, sl: [...]} per plugin). */
function nullPlugins() {
  const html = fs.readFileSync(PAGE, 'utf8');
  return eval(html.match(/const NULL_PLUGINS = (\[[\s\S]*?\n\]);/)[1]);
}

/** A new REAPER stand-in; its bridge runs every 30 ms until close(). */
function standin() {
  const L = lauxlib.luaL_newstate();
  lualib.luaL_openlibs(L);
  function run(code, name) {
    if (lauxlib.luaL_loadbuffer(L, to_luastring(code), null, to_luastring(name || 'chunk')) !== 0 || lua.lua_pcall(L, 0, 0, 0) !== 0) {
      throw new Error(lua.lua_tojsstring(L, -1));
    }
  }
  function call(fn, arg) {
    lua.lua_getglobal(L, to_luastring(fn));
    if (arg !== undefined) lua.lua_pushstring(L, to_luastring(String(arg)));
    if (lua.lua_pcall(L, arg !== undefined ? 1 : 0, 1, 0) !== 0) throw new Error(lua.lua_tojsstring(L, -1));
    const out = lua.lua_isnil(L, -1) ? null : lua.lua_tojsstring(L, -1);
    lua.lua_pop(L, 1);
    return out;
  }
  function q(expr) {
    run(`__q = tostring(${expr})`);
    lua.lua_getglobal(L, to_luastring('__q'));
    const v = lua.lua_tojsstring(L, -1);
    lua.lua_pop(L, 1);
    return v;
  }

  run(fs.readFileSync(path.join(__dirname, 'mock_reaper.lua'), 'utf8'), 'mock_reaper.lua');
  const delay = nullPlugins().find((p) => p.s === 'delay');
  const js = delay.sl.map((s) => [(s[7].includes('h') ? '-' : '') + s[0] + (s[1] ? ` (${s[1]})` : ''), s[5], s[2], s[3] || 1, s[6]]);
  js.push(['Wet', 1, 0, 1], ['Bypass', 0, 0, 1]);
  run(`catalog_add(${luaVal(['JS:null_jsfx/delay.jsfx'])}, "JS: NULL Delay", "JS: NULL Delay", "null_jsfx/delay.jsfx", ${luaVal(js)})
reaper.TrackFX_AddByName(tracks[1], "JS:null_jsfx/delay.jsfx", false, -1)`);
  run(fs.readFileSync(BRIDGE, 'utf8'), 'null_jsfx_remote_bridge.lua');
  const ticker = setInterval(() => call('tick'), 30);

  return {
    run, call, q,
    num: (expr) => parseFloat(q(expr)),
    /** REAPER's answer to a web interface request (the part after /_/). */
    web: (req) => call('web', req),
    close: () => clearInterval(ticker),
  };
}

module.exports = { standin, nullPlugins, luaVal };
