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

static HANDLE GetPipe() {
  const char* path = "\\\\.\\pipe\\com.fredemmott.openkneeboard.events.v1";
  HANDLE pipe = CreateFileA(
    path,
    GENERIC_WRITE,
    0,
    nullptr,
    OPEN_EXISTING,
    0,
    NULL);
  if (pipe == INVALID_HANDLE_VALUE) {
    CloseHandle(pipe);
    dprintf("Failed to open pipe: {}", GetLastError());
    return pipe;
  }

  if (!WaitNamedPipeA(path, 10 /* ms */)) {
    auto err = GetLastError();
    CloseHandle(pipe);
    dprintf("WaitNamedPipe failed: {}", err);
    return INVALID_HANDLE_VALUE;
  }

  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
    dprintf("Failed to set pipe state: {}", GetLastError());
    CloseHandle(pipe);
    return INVALID_HANDLE_VALUE;
  }

  return pipe;
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

  auto pipe = GetPipe();
  if (pipe == INVALID_HANDLE_VALUE) {
    lua_pushliteral(state, "Failed to open pipe.");
    lua_error(state);
    return 1;
  }

  const auto message = fmt::format(
    "com.fredemmott.openkneeboard.dcsext/{}", lua_tostring(state, 1));
  const std::string value(lua_tostring(state, 2));

  std::string packet = fmt::format(
    "{:08x}!{}!{:08x}!{}!", message.size(), message, value.size(), value);

  auto written = WriteFile(
    pipe, packet.data(), static_cast<DWORD>(packet.size()), nullptr, nullptr);
  auto err = GetLastError();
  CloseHandle(pipe);

  if (!written) {
    auto msg = fmt::format("Failed to write: {}", err);
    dprint(msg);
    lua_pushstring(state, msg.c_str());
    lua_error(state);
    return 1;
  }

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
