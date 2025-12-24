// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/APIEvent.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <cinttypes>
#include <cstdlib>
#include <format>
#include <string>

extern "C" {
#include <lauxlib.h>
}

using OpenKneeboard::dprint;

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.API.Lua")
 * 039d7b52-2065-5863-802b-873c638bdf88
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.API.Lua",
  (0x039d7b52, 0x2065, 0x5863, 0x80, 0x2b, 0x87, 0x3c, 0x63, 0x8b, 0xdf, 0x88));
}// namespace OpenKneeboard

static void push_arg_error(lua_State* state) {
  lua_pushliteral(state, "2 string arguments are required\n");
  lua_error(state);
}

static int SendToOpenKneeboard(lua_State* state) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(activity, "SendToOpenKneeboard");
  int argc = lua_gettop(state);
  if (argc != 2) {
    dprint("Invalid argument count\n");
    push_arg_error(state);
    activity.StopWithResult("InvalidArgs");
    return 1;
  }

  if (!(lua_isstring(state, 1) && lua_isstring(state, 2))) {
    dprint("Non-string args\n");
    push_arg_error(state);
    activity.StopWithResult("NonStringArgs");
    return 1;
  }

  const OpenKneeboard::APIEvent event {
    lua_tostring(state, 1),
    lua_tostring(state, 2),
  };
  event.Send();

  return 0;
}

extern "C" int __declspec(dllexport)
#if UINTPTR_MAX == UINT64_MAX
luaopen_OpenKneeboard_LuaAPI64(lua_State* state) {
#elif UINTPTR_MAX == UINT32_MAX
luaopen_OpenKneeboard_LuaAPI32(lua_State* state) {
#endif
  OPENKNEEBOARD_TraceLoggingScope("luaopen_OpenKneeboard_LuaAPI64");
  OpenKneeboard::DPrintSettings::Set({
    .prefix = "OpenKneeboard-LuaAPI",
  });
  lua_createtable(state, 0, 1);
  lua_pushcfunction(state, &SendToOpenKneeboard);
  lua_setfield(state, -2, "sendRaw");
  return 1;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  const auto& provider = OpenKneeboard::gTraceProvider;
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      TraceLoggingRegister(provider);
      TraceLoggingWrite(provider, "Attached", TraceLoggingThisExecutable());
      break;
    case DLL_PROCESS_DETACH:
      TraceLoggingWrite(provider, "Detached", TraceLoggingThisExecutable());
      TraceLoggingUnregister(provider);
      break;
  }
  return TRUE;
}
