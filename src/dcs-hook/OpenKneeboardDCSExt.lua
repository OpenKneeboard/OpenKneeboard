--[[
OpenKneeboard

Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
]]--
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
  selfData = nil,
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
  if (state.selfData) then
    OpenKneeboard.send("SelfData", net.lua2json(state.selfData))
  end
end

function callbacks.onMissionLoadBegin()
  l("onMissionLoadBegin: "..DCS.getMissionName())
  if DCS.isMultiplayer() then
    l("Mission: is multiplayer: ")
    -- search for the right track:
    local mpTrackPath = lfs.writedir() .. "\\Tracks\\Multiplayer";
    found = {}
    for entry in lfs.dir(mpTrackPath) do
      if string.find(entry, DCS.getMissionName()) then
        l("Found: " .. entry)
        table.insert(found, entry)
      end
    end
    table.sort(found)
    if #found > 0 then
      l("Setting Mission: " .. found[#found])
      state.mission = mpTrackPath .. "\\" .. found[#found]
    end
  else 
    local file = DCS.getMissionFilename()
    file = file:gsub("^.[/\\]+", lfs.currentdir())
    state.mission = file
    l("Mission: "..state.mission)
  end
  local mission = DCS.getCurrentMission()
  if mission and mission.mission and mission.mission.theatre then
    state.terrain = mission.mission.theatre
    l("Terrain: "..state.terrain)
  end
  state.selfData = Export.LoGetSelfData()
  sendState()
end

function callbacks.onSimulationStart()
  l("onSimulationStart")

  local selfData = Export.LoGetSelfData()
  if not selfData then
    OpenKneeboard.send("SimulationStart", "");
    return
  end
  state.aircraft = selfData.Name
  state.selfData = selfData
  l("Aircraft: "..state.aircraft)
  sendState()
  OpenKneeboard.send("SimulationStart", "");
end

function callbacks.onPlayerChangeSlot(id)
  if id == net.get_my_player_id() then
    local slotid = net.get_player_info(id, 'slot')
    state.aircraft = DCS.getUnitProperty(slotid, DCS.UNIT_TYPE)
    state.selfData = Export.LoGetSelfData()
    sendState()
  end
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
