function l(msg)
  log.write("YAVRK", log.INFO, msg)
end
l("Init")

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

state = {
  mission = nil,
  aircraft = nil,
  terrain = nil,
}

function sendState()
  if state.aircraft then
    YAVRK.send("Aircraft", state.aircraft)
  end
  if state.mission then
    YAVRK.send("Mission", state.mission)
  end
  if state.terrain then
    YAVRK.send("Terrain", state.terrain)
  end
end

function callbacks.onMissionLoadBegin()
  l("onMissionLoadBegin: "..DCS.getMissionName())
  local file = DCS.getMissionFilename()
  file = file:gsub("^.[/\\]+", lfs.currentdir())
  state.mission = file;

  local mission = DCS.getCurrentMission();
  if mission and mission.mission and mission.mission.theatre then
    state.terrain = mission.mission.theatre
  end
  sendState()
end

function callbacks.onSimulationStart()
  l("onSimulationStart")

  local selfData = Export.LoGetSelfData()
  if not selfData then
    return
  end
  l("onSimulationStart in aircraft "..selfData.Name)
  state.aircraft = selfData.name
  sendState()
end

function callbacks.onGameEvent(event, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
  l("Game event: "..event)
end

lastUpdate = DCS.getModelTime()
function callbacks.onSimulationFrame()
  if DCS.getModelTime() < lastUpdate + 1 then
    return
  end
  lastUpdate = DCS.getModelTime()
  sendState()
end

DCS.setUserCallbacks(callbacks)

l("Callbacks installed")
