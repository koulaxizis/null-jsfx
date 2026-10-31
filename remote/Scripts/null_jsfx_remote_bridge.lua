-- NULL JSFX Remote: bridge between REAPER and the tablet page (null_jsfx.html)
-- NULL JSFX v2.8 • https://nulljsfx.tech • MIT License
--
-- REAPER's web interface can reach tracks (volume, pan, mute, solo...) but not the FX on
-- them. This script runs in the background inside REAPER and passes FX data through
-- REAPER's ExtState, which the web interface can read and write:
--
--   in_<slot>   written by the page: "seq;command;command..." (see run_command)
--   out_<slot>  written here: JSON with the chain of the viewed track and the
--               parameters of the open FX, plus "ack" = the last seq applied
--   alive       written here every half second: "<version>|<time>"
--
-- A slot is one tablet (null_jsfx.html?slot=2 for a second one). The page resends every
-- change it has not seen acknowledged, so a request that overwrites the previous one before
-- this script read it loses nothing. Setting a value twice is harmless; commands that must
-- run once (add, move, remove) carry an id ("!<id>:add,...") and run only the first time.
-- Tracks and FX are named by GUID, so moving them while the page is open can not send a
-- change to the wrong one.

local SEC = "NULL_JSFX_REMOTE"
local VERSION = "1"
local SLOTS = 4
local MAX_PARAMS = 256
local IDLE_AFTER = 6 -- seconds without a request before a slot stops being updated

local r = reaper
local done, done_order = {}, {} -- ids of the run-once commands already run
local slots = {}
for s = 1, SLOTS do slots[s] = { seq = nil, seen = -1e9, track = -1, open = nil, added = nil, out = nil } end

-- JSON ---------------------------------------------------------------------------------------

local function json_str(s)
  s = tostring(s or "")
  s = s:gsub('[%c"\\]', function(c)
    if c == '"' then return '\\"' end
    if c == "\\" then return "\\\\" end
    if c == "\n" then return "\\n" end
    if c == "\t" then return "\\t" end
    return string.format("\\u%04x", c:byte())
  end)
  return '"' .. s .. '"'
end

local function json_num(v)
  if v ~= v or v == math.huge or v == -math.huge then return "0" end
  return string.format("%.6g", v)
end

-- Tracks and FX ------------------------------------------------------------------------------

-- index as in the web interface: 0 = master, 1 = first track
local function track_at(idx)
  if idx == 0 then return r.GetMasterTrack(0) end
  return r.GetTrack(0, idx - 1)
end

local function track_by_guid(guid)
  if not guid or guid == "" then return nil end
  local master = r.GetMasterTrack(0)
  if r.GetTrackGUID(master) == guid then return master, 0 end
  for i = 0, r.CountTracks(0) - 1 do
    local tr = r.GetTrack(0, i)
    if r.GetTrackGUID(tr) == guid then return tr, i + 1 end
  end
  return nil
end

local function fx_by_guid(tr, guid)
  if not tr or not guid then return nil end
  for i = 0, r.TrackFX_GetCount(tr) - 1 do
    if r.TrackFX_GetFXGUID(tr, i) == guid then return i end
  end
  return nil
end

local function named(tr, fx, key)
  local ok, v = r.TrackFX_GetNamedConfigParm(tr, fx, key)
  return ok and v or ""
end

-- Commands from the page ---------------------------------------------------------------------

local function split(s, sep)
  local out = {}
  for part in (s .. sep):gmatch("(.-)" .. sep) do out[#out + 1] = part end
  return out
end

-- The ways to add a NULL plugin of each format, tried in order (the file path for JSFX
-- avoids "NULL Delay" also matching "NULL Delay GFX").
local function add_names(stem, format, name)
  if format == "gfx" then return { "JS:null_jsfx/gfx/" .. stem .. "_gfx.jsfx", "JS:NULL " .. name .. " GFX" } end
  if format == "vst3" then return { "VST3:" .. name .. " (NULL JSFX)" } end
  if format == "clap" then return { "CLAP:" .. name .. " (NULL JSFX)" } end
  return { "JS:null_jsfx/" .. stem .. ".jsfx", "JS:NULL " .. name }
end

local function run_command(slot, f, id)
  local cmd = f[1]
  if cmd == "view" then -- view,<track index as in the web interface, -1 = none>,<open fx guid>
    slot.track = tonumber(f[2]) or -1
    slot.open = f[3] ~= "" and f[3] or nil
    return
  end
  local tr = track_by_guid(f[2])
  if not tr then return end
  if cmd == "add" then -- add,<track>,<stem>,<format>,<name>
    if not f[3]:match("^[%w_]+$") then return end
    r.Undo_BeginBlock()
    local idx = -1
    for _, n in ipairs(add_names(f[3], f[4], f[5] or "")) do
      idx = r.TrackFX_AddByName(tr, n, false, -1)
      if idx >= 0 then break end
    end
    r.Undo_EndBlock("NULL JSFX Remote: add " .. (f[5] or f[3]), -1)
    if idx >= 0 and id then slot.added = id .. "|" .. r.TrackFX_GetFXGUID(tr, idx) end
    return
  end
  local fx = fx_by_guid(tr, f[3])
  if not fx then return end
  if cmd == "p" then -- p,<track>,<fx>,<param>,<value>: real value
    r.TrackFX_SetParam(tr, fx, tonumber(f[4]), tonumber(f[5]))
  elseif cmd == "n" then -- n,<track>,<fx>,<param>,<0..1>
    r.TrackFX_SetParamNormalized(tr, fx, tonumber(f[4]), tonumber(f[5]))
  elseif cmd == "on" then -- on,<track>,<fx>,<0|1>
    r.TrackFX_SetEnabled(tr, fx, f[4] == "1")
  elseif cmd == "show" then -- show,<track>,<fx>: open its window on the computer
    r.TrackFX_Show(tr, fx, 3)
  elseif cmd == "mv" then -- mv,<track>,<fx>,<new index>
    local to = tonumber(f[4])
    if to and to >= 0 and to < r.TrackFX_GetCount(tr) and to ~= fx then
      r.Undo_BeginBlock()
      r.TrackFX_CopyToTrack(tr, fx, tr, to, true)
      r.Undo_EndBlock("NULL JSFX Remote: move FX", -1)
    end
  elseif cmd == "del" then -- del,<track>,<fx>
    r.Undo_BeginBlock()
    r.TrackFX_Delete(tr, fx)
    r.Undo_EndBlock("NULL JSFX Remote: remove FX", -1)
  end
end

-- A request left over from before this script started is not replayed.
local function skip_old_requests()
  for s = 1, SLOTS do
    local cmds = split(r.GetExtState(SEC, "in_" .. s), ";")
    slots[s].seq = tonumber(cmds[1])
    for i = 2, #cmds do
      local id = cmds[i]:match("^!([%w]+):")
      if id then
        done[id] = true
        done_order[#done_order + 1] = id
      end
    end
  end
end

local function read_requests(s, slot, now)
  local req = r.GetExtState(SEC, "in_" .. s)
  if req == "" then return false end
  local cmds = split(req, ";")
  local seq = tonumber(cmds[1])
  if not seq or seq == slot.seq then return false end
  slot.seq, slot.seen = seq, now
  for i = 2, #cmds do
    local c = cmds[i]
    local id, rest = c:match("^!([%w]+):(.*)$")
    if id then
      if done[id] then c = "" else
        done[id] = true
        done_order[#done_order + 1] = id
        if #done_order > 500 then done[table.remove(done_order, 1)] = nil end
        c = rest
      end
    end
    if c ~= "" then
      local ok, err = pcall(run_command, slot, split(c, ","), id)
      if not ok then r.ShowConsoleMsg("NULL JSFX Remote: " .. tostring(err) .. "\n") end
    end
  end
  return true
end

-- State for the page -------------------------------------------------------------------------

local function describe(slot)
  local parts = { '{"v":', VERSION, ',"ack":', tostring(slot.seq or -1) }
  if slot.added then parts[#parts + 1] = ',"added":' .. json_str(slot.added) end
  local tr = slot.track and slot.track >= 0 and track_at(slot.track)
  if tr then
    local _, name = r.GetTrackName(tr)
    parts[#parts + 1] = ',"track":{"i":' .. slot.track .. ',"g":' .. json_str(r.GetTrackGUID(tr)) .. ',"name":' .. json_str(name) .. "}"
    local fxs, open = {}, nil
    for i = 0, r.TrackFX_GetCount(tr) - 1 do
      local g = r.TrackFX_GetFXGUID(tr, i)
      local _, name = r.TrackFX_GetFXName(tr, i, "")
      fxs[#fxs + 1] = "{" .. table.concat({
        '"g":' .. json_str(g),
        '"name":' .. json_str(name),
        '"orig":' .. json_str(named(tr, i, "fx_name")),
        '"ident":' .. json_str(named(tr, i, "fx_ident")),
        '"on":' .. (r.TrackFX_GetEnabled(tr, i) and "true" or "false"),
        '"np":' .. r.TrackFX_GetNumParams(tr, i),
      }, ",") .. "}"
      if g == slot.open then open = i end
    end
    parts[#parts + 1] = ',"fx":[' .. table.concat(fxs, ",") .. "]"
    if open then
      local ps = {}
      for p = 0, math.min(r.TrackFX_GetNumParams(tr, open), MAX_PARAMS) - 1 do
        local v, lo, hi = r.TrackFX_GetParam(tr, open, p)
        local _, pname = r.TrackFX_GetParamName(tr, open, p, "")
        local _, fmt = r.TrackFX_GetFormattedParamValue(tr, open, p, "")
        ps[#ps + 1] = "[" .. table.concat({
          json_str(pname), json_num(v), json_num(lo), json_num(hi),
          json_num(r.TrackFX_GetParamNormalized(tr, open, p)), json_str(fmt),
        }, ",") .. "]"
      end
      parts[#parts + 1] = ',"open":{"g":' .. json_str(slot.open) .. ',"params":[' .. table.concat(ps, ",") .. "]}"
    end
  end
  parts[#parts + 1] = "}"
  return table.concat(parts)
end

-- Main loop ----------------------------------------------------------------------------------

local last_alive, tick = -1e9, 0

local function loop()
  local now = r.time_precise()
  tick = tick + 1
  if now - last_alive >= 0.5 then
    r.SetExtState(SEC, "alive", VERSION .. "|" .. string.format("%.3f", now), false)
    last_alive = now
  end
  for s = 1, SLOTS do
    local slot = slots[s]
    local changed = read_requests(s, slot, now)
    -- about 10 updates a second, or at once after a request
    if now - slot.seen < IDLE_AFTER and (changed or tick % 3 == 0) then
      local out = describe(slot)
      if out ~= slot.out then
        r.SetExtState(SEC, "out_" .. s, out, false)
        slot.out = out
      end
    end
  end
  r.defer(loop)
end

skip_old_requests()
r.atexit(function()
  r.DeleteExtState(SEC, "alive", false)
  for s = 1, SLOTS do r.DeleteExtState(SEC, "out_" .. s, false) end
end)

loop()
