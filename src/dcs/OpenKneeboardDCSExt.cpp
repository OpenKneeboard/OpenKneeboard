#include <Windows.h>
#include <fmt/format.h>
#include <winrt/base.h>

#include <string>

#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/dprint.h"

extern "C" {
#include <lauxlib.h>
}

using OpenKneeboard::dprint;
using OpenKneeboard::dprintf;

static const char g_Path[]
  = "\\\\.\\mailslot\\com.fredemmott.openkneeboard.events.v1";

static void push_arg_error(lua_State* state) {
  lua_pushliteral(state, "2 string arguments are required\n");
  lua_error(state);
}

static int SendToOpenKneeboard(lua_State* state) {
  int argc = lua_gettop(state);
  if (argc != 2) {
    dprint("Invalid argument count\n");
    push_arg_error(state);
    return 1;
  }

  if (!(lua_isstring(state, 1) && lua_isstring(state, 2))) {
    dprint("Non-string args\n");
    push_arg_error(state);
    return 1;
  }

  (OpenKneeboard::GameEvent {
    .Name = fmt::format(
      "com.fredemmott.openkneeboard.dcsext/{}", lua_tostring(state, 1)),
    .Value = lua_tostring(state, 2)}).Send();

  return 0;
}

extern "C" int __declspec(dllexport)
  luaopen_OpenKneeboardDCSExt(lua_State* state) {
  OpenKneeboard::DPrintSettings::Set({
    .Prefix = "OpenKneeboard-DCSExt",
  });
  lua_createtable(state, 0, 1);
  lua_pushcfunction(state, &SendToOpenKneeboard);
  lua_setfield(state, -2, "send");
  return 1;
}
