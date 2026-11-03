-- BEGIN NULL JSFX Remote: starts the bridge of the tablet remote and Remote for DAWs with REAPER
-- (the NULL JSFX installers and Hub add these lines, and take them out with the remote)
do local f = reaper.GetResourcePath() .. "/Scripts/null_jsfx_remote_bridge.lua"; if reaper.file_exists(f) then pcall(dofile, f) end end
-- END NULL JSFX Remote
