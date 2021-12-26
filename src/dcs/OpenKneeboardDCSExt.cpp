#include <Windows.h>
#include <fmt/format.h>

#include <string>

#include "OpenKneeboard/dprint.h"

extern "C" {
#include <lauxlib.h>
}

using OpenKneeboard::dprint;
using OpenKneeboard::dprintf;

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

  const auto message = fmt::format(
    "com.fredemmott.openkneeboard.dcsext/{}", lua_tostring(state, 1));
  const std::string value(lua_tostring(state, 2));

  std::string packet = fmt::format(
    "{:08x}!{}!{:08x}!{}!", message.size(), message, value.size(), value);

  HANDLE pipe = CreateFileA(
    "\\\\.\\pipe\\com.fredemmott.openkneeboard.events.v1",
    GENERIC_WRITE,
    0,
    nullptr,
    OPEN_EXISTING,
    0,
    NULL);
  if (!pipe) {
    auto msg = fmt::format("Failed to open pipe: {}", GetLastError());
    lua_pushstring(state, msg.c_str());
    lua_error(state);
    return 1;
  }

  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
    auto msg = fmt::format("Failed to set pipe state: {}", GetLastError());
    CloseHandle(pipe);
    lua_pushstring(state, msg.c_str());
    lua_error(state);
    return 1;
  }

  auto written
    = WriteFile(pipe, packet.data(), packet.size(), nullptr, nullptr);
  auto err = GetLastError();
  CloseHandle(pipe);
  if (!written) {
    auto msg = fmt::format("Failed to write: {}", err);
    lua_pushstring(state, msg.c_str());
    lua_error(state);
  }

  return 0;
}

extern "C" int __declspec(dllexport)
  luaopen_OpenKneeboardDCSExt(lua_State* state) {
  OpenKneeboard::DPrintSettings::Set({
    .Prefix = "OpenKneeboard-DCSExt",
    .Target = OpenKneeboard::DPrintSettings::Target::DEBUG_STREAM,
  });
  lua_createtable(state, 0, 1);
  lua_pushcfunction(state, &SendToOpenKneeboard);
  lua_setfield(state, -2, "send");
  return 1;
}
