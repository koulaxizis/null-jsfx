-- A small stand-in for REAPER, for tests/remote/remote_test.mjs: a project with tracks and
-- FX, the ReaScript functions the bridge uses (reaper.*) and the web interface commands the
-- tablet page sends (web()). FX catalog entries come from the test (catalog_add).

local now = 0
local deferred = {}
local extstate = {}
local next_guid = 100
local catalog = {} -- name to add by -> function returning a new FX
weblog = {}
console = {}

local function guid()
  next_guid = next_guid + 1
  return string.format("{00000000-0000-0000-0000-%012d}", next_guid)
end

local function new_track(name)
  return { name = name, guid = guid(), vol = 1, pan = 0, mute = false, solo = false, arm = false, sel = false, fx = {}, peak = -1500 }
end

master = new_track("MASTER")
tracks = { new_track("Vocals"), new_track("Bass"), new_track("Drums") }
transport = { state = 0, pos = 0, repeat_ = false }

-- params: { name, value, min, max, format(value) } ; JSFX use their own units, VST3/CLAP 0..1
function catalog_add(names, display, fx_name, ident, params)
  for _, n in ipairs(names) do
    catalog[n] = function()
      local fx = { guid = guid(), name = display, fx_name = fx_name, ident = ident, enabled = true, params = {} }
      for i, p in ipairs(params) do fx.params[i] = { name = p[1], value = p[2], min = p[3], max = p[4], options = p[5] } end
      return fx
    end
  end
end

local function fmt_param(p)
  if p.options then return p.options[math.floor(p.value - p.min + 0.5) + 1] or "" end
  return string.format("%.1f", p.value)
end

-- reaper.* ------------------------------------------------------------------------------------

reaper = {}
local r = reaper
function r.time_precise() return now end
function r.defer(f) deferred[#deferred + 1] = f end
function r.atexit(f) end
function r.ShowConsoleMsg(s) console[#console + 1] = s end
function r.GetExtState(sec, key) return extstate[sec .. "/" .. key] or "" end
function r.SetExtState(sec, key, value, persist) extstate[sec .. "/" .. key] = value end
function r.DeleteExtState(sec, key) extstate[sec .. "/" .. key] = nil end
function r.GetMasterTrack() return master end
function r.GetTrack(_, i) return tracks[i + 1] end
function r.CountTracks() return #tracks end
function r.GetTrackGUID(tr) return tr.guid end
function r.GetTrackName(tr) return true, tr.name end
function r.TrackFX_GetCount(tr) return #tr.fx end
function r.TrackFX_GetFXGUID(tr, i) return tr.fx[i + 1] and tr.fx[i + 1].guid end
function r.TrackFX_GetFXName(tr, i) return true, tr.fx[i + 1].name end
function r.TrackFX_GetNamedConfigParm(tr, i, key)
  local fx = tr.fx[i + 1]
  if key == "fx_name" then return true, fx.fx_name end
  if key == "fx_ident" then return true, fx.ident end
  return false, ""
end
function r.TrackFX_GetEnabled(tr, i) return tr.fx[i + 1].enabled end
function r.TrackFX_SetEnabled(tr, i, on) tr.fx[i + 1].enabled = on end
function r.TrackFX_GetNumParams(tr, i) return #tr.fx[i + 1].params end
function r.TrackFX_GetParam(tr, i, p)
  local q = tr.fx[i + 1].params[p + 1]
  return q.value, q.min, q.max
end
function r.TrackFX_GetParamNormalized(tr, i, p)
  local q = tr.fx[i + 1].params[p + 1]
  return (q.value - q.min) / (q.max - q.min)
end
function r.TrackFX_GetParamName(tr, i, p) return true, tr.fx[i + 1].params[p + 1].name end
function r.TrackFX_GetFormattedParamValue(tr, i, p) return true, fmt_param(tr.fx[i + 1].params[p + 1]) end
function r.TrackFX_SetParam(tr, i, p, v)
  local q = tr.fx[i + 1].params[p + 1]
  q.value = math.min(q.max, math.max(q.min, v))
  return true
end
function r.TrackFX_SetParamNormalized(tr, i, p, n)
  local q = tr.fx[i + 1].params[p + 1]
  q.value = q.min + math.min(1, math.max(0, n)) * (q.max - q.min)
  return true
end
function r.TrackFX_AddByName(tr, name, rec, pos)
  local make = catalog[name]
  if not make then return -1 end
  tr.fx[#tr.fx + 1] = make()
  return #tr.fx - 1
end
function r.TrackFX_Delete(tr, i) table.remove(tr.fx, i + 1) return true end
function r.TrackFX_CopyToTrack(src, i, dst, to, move)
  local fx = table.remove(src.fx, i + 1)
  table.insert(dst.fx, to + 1, fx)
end
shown = {}
function r.TrackFX_Show(tr, i, how) shown[#shown + 1] = tr.fx[i + 1].guid end
undo = {}
function r.Undo_BeginBlock() end
function r.Undo_EndBlock(s) undo[#undo + 1] = s end

function tick(seconds)
  now = now + (seconds or 0.03)
  local fs = deferred
  deferred = {}
  for _, f in ipairs(fs) do f() end
end

-- web interface -------------------------------------------------------------------------------

local function decode(s)
  return (s:gsub("%%(%x%x)", function(h) return string.char(tonumber(h, 16)) end))
end

local function escape(s)
  return (s:gsub("\\", "\\\\"):gsub("\t", "\\t"):gsub("\n", "\\n"))
end

local function track_line(i, t)
  local flags = (t.sel and 2 or 0) + (#t.fx > 0 and 4 or 0) + (t.mute and 8 or 0) + (t.solo and 16 or 0) + (t.arm and 64 or 0)
  return table.concat({ "TRACK", i, t.name, flags, string.format("%.6f", t.vol), string.format("%.6f", t.pan),
    t.peak, t.peak, 1, 0, 0, 0, 0, 0 }, "\t")
end

local function track_at(i) if i == 0 then return master end return tracks[i] end

local function toggle(cur, v) if tonumber(v) < 0 then return not cur end return tonumber(v) > 0 end

function web(req)
  local out = {}
  for cmd in (req .. ";"):gmatch("(.-);") do
    weblog[#weblog + 1] = cmd
    local f = {}
    for part in (cmd .. "/"):gmatch("(.-)/") do f[#f + 1] = decode(part) end
    if cmd == "TRANSPORT" then
      out[#out + 1] = table.concat({ "TRANSPORT", transport.state, string.format("%.6f", transport.pos), transport.repeat_ and 1 or 0, "1.1.00", "1.1.00" }, "\t")
    elseif cmd == "TRACK" then
      out[#out + 1] = track_line(0, master)
      for i, t in ipairs(tracks) do out[#out + 1] = track_line(i, t) end
    elseif f[1] == "GET" and f[2] == "EXTSTATE" then
      out[#out + 1] = table.concat({ "EXTSTATE", f[3], f[4], escape(extstate[f[3] .. "/" .. f[4]] or "") }, "\t")
    elseif f[1] == "SET" and f[2] == "EXTSTATE" then
      extstate[f[3] .. "/" .. f[4]] = f[5]
    elseif f[1] == "SET" and f[2] == "TRACK" then
      local t = track_at(tonumber(f[3]))
      if f[4] == "VOL" then t.vol = tonumber(f[5])
      elseif f[4] == "PAN" then t.pan = tonumber(f[5])
      elseif f[4] == "MUTE" then t.mute = toggle(t.mute, f[5])
      elseif f[4] == "SOLO" then t.solo = toggle(t.solo, f[5])
      elseif f[4] == "RECARM" then t.arm = toggle(t.arm, f[5]) end
    elseif f[1] == "SET" and f[2] == "REPEAT" then
      transport.repeat_ = toggle(transport.repeat_, f[3])
    elseif cmd == "1007" then transport.state = 1
    elseif cmd == "1008" then transport.state = 2
    elseif cmd == "1016" then transport.state = 0
    elseif cmd == "1013" then transport.state = 5
    end
  end
  return table.concat(out, "\n") .. "\n"
end
