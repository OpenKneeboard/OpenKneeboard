function l(msg)
  log.write("YAVRK", log.INFO, msg)
end
l("Init")

--[[
local source_file = debug.getinfo(1, "S").source
local source_dir = source_file:gsub("[/\\]+[^/\\]+$", ""):gsub("/", "\\")

package.cpath = source_dir.."\\?.dll;"..package.cpath
--]]
package.cpath = lfs.writedir().."\\Scripts\\Hooks\\?.dll;"..package.cpath;
l("Loading DLL - search path: "..package.cpath)
local status, ret = pcall(require, "YAVRKDCSExt")
if status then
  l("DLL Loaded")
else
  l("Failed: "..err)
  return
end

YAVRK = ret

local callbacks = {}

function callbacks.onMissionLoadBegin()
  l("onMissionLoadBegin: "..DCS.getMissionName())
  local file = DCS.getMissionFilename()
  file = file:gsub("^.[/\\]+", lfs.currentdir())
  YAVRK.send("Mission", file)

  local mission = DCS.getCurrentMission();
  if mission and mission.mission and mission.mission.theatre then
    YAVRK.send("Theatre", mission.mission.theatre)
  end
end

function callbacks.onSimulationStart()
  l("onSimulationStart")

  local selfData = Export.LoGetSelfData()
  if not selfData then
    return
  end
  l("onSimulationStart in aircraft "..selfData.Name)
  YAVRK.send("Aircraft", selfData.Name)
end

function callbacks.onGameEvent(event, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
  l("Game event: "..event)
end

DCS.setUserCallbacks(callbacks)

l("Callbacks installed")
