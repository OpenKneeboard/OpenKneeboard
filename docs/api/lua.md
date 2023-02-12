# Lua API

## Overview

The Lua API is able to send information, events, or requests to OpenKneeboard; it is not able to receive data from OpenKneeboard. It requires a Lua extension - `OpenKneeboard_LuaAPI64.dll` - which changes with every version of OpenKneeboard.

This DLL is installed and kept up to date in:

- `C:\Program Files\OpenKneeboard\bin\OpenKneeboard_LuaAPI64.dll`
- `Scripts\Hooks\OpenKneeboard_LuaAPI64.dll` within your DCS saved games path

If you're using it outside of DCS, or outside of `Scripts\Hooks`, you most likely want to load it directly from program files.

**In OpenKneeboard v1.3 and below** the DLL is `OpenKneeboardDCSExt.dll` instead of `OpenKneeboard_LuaAPI64.dll`; there is no 32-bit version.

## Game Compatibility

OpenKneeboard's Lua API is only tested with DCS world; the DLL provided with OpenKneeboard requires that the game:
- uses Lua 5.1.5
- uses `lua.dll`, not `lua51.dll`

Supporting other games will require manually rebuilding OpenKneeboard, and changing the version or names of the DLLs in OpenKneeboard's build system. This is not currently supported. I'm open to pull requests adding other Lua build configurations.

32-bit games have the same requirement, but should load `OpenKneeboard_LuaAPI32` instead.

## Usage

```lua
--[[
	********************************
	**** 1. Set `package.cpath` ****
	********************************

	Tell Lua where to look for the DLLs; replace `SOME_FOLDER` with
	the path to a folder containing an up-to-date `OpenKneeboard_LuaAPI64.dll`
--]]
package.cpath = "SOME_DIRECTORY\\?.dll;"..package.cpath
-- For example, in DCS:
package.cpath = lfs.writedir().."\\Scripts\\Hooks\\?.dll;"..package.cpath

--[[
	**************************
	**** 2. Load the DLL *****
	**************************
]]--
local status, OpenKneeboard = pcall(require, "OpenKneeboard_LuaAPI64")
if status then
  l("DLL Loaded")
else
  l("Failed: "..err)
  return
end

--[[
	******************************************
	**** 3. Send messages to OpenKneeboard ***
	******************************************
]]--

OpenKneeboard.sendRaw(
	"RemoteUserAction",
	"NEXT_TAB")
```

## Messages

See [the messages documentation](messages.md) for information on supported messages.

## Sending JSON from DCS

OpenKneeboard needs JSON data for some messages; in DCS, use `net.lua2json()`, e.g.:

```lua
-- For JSON in DCS LUA:
function OpenKneeboard.sendJSON(name, value)
  OpenKneeboard.sendRaw(
	""..name,
	net.lua2json(value))
end

-- For example:
OpenKneeboard.sendJSON(
	"SetTabByName",
	{
		Name = "Some Tab Title",
		Kneeboard = 2,
		PageNumber = 123,
	})
```
## Examples

- [OpenKneeboard's DCS extension](../../src/dcs-hook/OpenKneeboardDCSExt.lua)
