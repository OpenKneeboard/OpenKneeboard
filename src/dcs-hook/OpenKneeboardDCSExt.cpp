/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>
#include <Windows.h>
#include <shims/winrt.h>

#include <format>
#include <string>

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

  auto ge = OpenKneeboard::GameEvent {
    std::format(
      "com.fredemmott.openkneeboard.dcsext/{}", lua_tostring(state, 1)),
    lua_tostring(state, 2)};
  ge.Send();

  return 0;
}

extern "C" int __declspec(dllexport)
  luaopen_OpenKneeboardDCSExt(lua_State* state) {
  OpenKneeboard::DPrintSettings::Set({
    .prefix = "OpenKneeboard-DCSExt",
  });
  lua_createtable(state, 0, 1);
  lua_pushcfunction(state, &SendToOpenKneeboard);
  lua_setfield(state, -2, "send");
  return 1;
}
