#include <Windows.h>
#include <fmt/format.h>
#include <winrt/base.h>

#include <string>

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

  winrt::file_handle handle {CreateFileA(
    g_Path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, NULL)};
  if (!handle) {
    const auto err = fmt::format("Failed to open mailslot: {}", GetLastError());
    dprint(err);
    lua_pushstring(state, err.c_str());
    lua_error(state);
    return 1;
  }

  const auto message = fmt::format(
    "com.fredemmott.openkneeboard.dcsext/{}", lua_tostring(state, 1));
  const std::string value(lua_tostring(state, 2));

  std::string packet = fmt::format(
    "{:08x}!{}!{:08x}!{}!", message.size(), message, value.size(), value);

  if (!WriteFile(
        handle.get(),
        packet.data(),
        static_cast<DWORD>(packet.size()),
        nullptr,
        nullptr)) {
    const auto err = fmt::format("Failed to write to mailslot: {}", GetLastError());
    dprint(err);
    lua_pushstring(state, err.c_str());
    lua_error(state);
    return 1;
  }

  dprintf("Wrote to mailslot: {}", packet);

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
