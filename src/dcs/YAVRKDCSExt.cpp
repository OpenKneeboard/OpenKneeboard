#include <Windows.h>
#include <fmt/format.h>

#include <string>

extern "C" {
#include <lauxlib.h>
}

static void push_arg_error(lua_State* state) {
  lua_pushliteral(state, "2 string arguments are required\n");
  lua_error(state);
}

static int SendToYAVRK(lua_State* state) {
  OutputDebugStringA(__FUNCTION__);
  int argc = lua_gettop(state);
  if (argc != 2) {
    OutputDebugStringA("Invalid argument count\n");
    push_arg_error(state);
    return 1;
  }

  if (!(lua_isstring(state, 1) && lua_isstring(state, 2))) {
    OutputDebugStringA("Non-string args\n");
    push_arg_error(state);
    return 1;
  }

  const auto message
    = fmt::format("com.fredemmott.yavrk.dcsext/{}", lua_tostring(state, 1));
  const std::string value(lua_tostring(state, 2));

  std::string packet = fmt::format(
    "{:08x}!{}!{:08x}!{}!", message.size(), message, value.size(), value);

  OutputDebugStringA(packet.c_str());

  const auto success = CallNamedPipeA(
    "\\\\.\\pipe\\com.fredemmott.yavrk.events.v1",
    packet.data(),
    DWORD(packet.size()),
    nullptr,
    NULL,
    nullptr,
    NMPWAIT_NOWAIT);
  if (success) {
    OutputDebugStringA("Sent to YAVRK.\n");
  } else {
    OutputDebugStringA("YAVRK unreachable.\n");
  }

  return 0;
}

extern "C" int __declspec(dllexport) luaopen_YAVRKDCSExt(lua_State* state) {
  OutputDebugStringA(__FUNCTION__);
  lua_createtable(state, 0, 1);
  lua_pushcfunction(state, &SendToYAVRK);
  lua_setfield(state, -2, "send");
  return 1;
}
