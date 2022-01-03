function l(msg)
  log.write("OpenKneeboard", log.INFO, msg)
end
l("Init")

package.cpath = lfs.writedir().."\\Scripts\\Hooks\\?.dll;"..package.cpath
l("Loading DLL - search path: "..package.cpath)
local status, ret = pcall(require, "OpenKneeboardDCSExt")
if status then
  l("DLL Loaded")
else
  l("Failed: "..err)
  return
end

OpenKneeboard = ret

local callbacks = {}

state = {
  mission = nil,
  aircraft = nil,
  terrain = nil,
}

function sendState()
  OpenKneeboard.send("InstallPath", lfs.currentdir());
  OpenKneeboard.send("SavedGamesPath", lfs.writedir());
  if state.aircraft then
    OpenKneeboard.send("Aircraft", state.aircraft)
  end
  if state.mission then
    OpenKneeboard.send("Mission", state.mission)
  end
  if state.terrain then
    OpenKneeboard.send("Terrain", state.terrain)
  end
end

function callbacks.onMissionLoadBegin()
  l("onMissionLoadBegin: "..DCS.getMissionName())
  local file = DCS.getMissionFilename()
  file = file:gsub("^.[/\\]+", lfs.currentdir())
  state.mission = file
  l("Mission: "..state.mission)

  local mission = DCS.getCurrentMission()
  if mission and mission.mission and mission.mission.theatre then
    state.terrain = mission.mission.theatre
    l("Terrain: "..state.terrain)
  end
  sendState()
end

function callbacks.onSimulationStart()
  l("onSimulationStart")

  local selfData = Export.LoGetSelfData()
  if not selfData then
    return
  end
  state.aircraft = selfData.Name
  l("Aircraft: "..state.aircraft)
  sendState()
  OpenKneeboard.send("SimulationStart", "");
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

function callbacks.onRadioMessage(message, duration)
  OpenKneeboard.send("RadioMessage", message)
end

function callbacks.onShowMessage(message, duration)
  OpenKneeboard.send("RadioMessage", "[show]"..message);
end

function callbacks.onTriggerMessage(message, duration, clearView)
  if clearView then
    return
  end
  OpenKneeboard.send("RadioMessage", ">> "..message);
end

DCS.setUserCallbacks(callbacks)

l("Callbacks installed")
