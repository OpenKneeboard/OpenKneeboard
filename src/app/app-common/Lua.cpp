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
#pragma once

#include <OpenKneeboard/Lua.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>

#include <algorithm>
#include <format>

namespace OpenKneeboard::detail {

class LuaStateImpl final {
 public:
  LuaStateImpl() {
    mLua = lua_open();
    luaL_openlibs(mLua);
  }
  LuaStateImpl(const LuaStateImpl&) = delete;
  LuaStateImpl(LuaStateImpl&&) = delete;
  LuaStateImpl& operator=(const LuaStateImpl&) = delete;
  LuaStateImpl& operator=(LuaStateImpl&&) = delete;

  ~LuaStateImpl() {
    lua_close(mLua);
  }

  operator lua_State*() const noexcept {
    return mLua;
  }

 private:
  lua_State* mLua = nullptr;
};

class LuaRefImpl final {
 public:
  LuaRefImpl(const std::shared_ptr<LuaStateImpl>& lua) : mLua(lua) {
    mType = static_cast<LuaType>(lua_type(*lua, -1));
    mRef = luaL_ref(*lua, LUA_REGISTRYINDEX);
  }

  ~LuaRefImpl() {
    luaL_unref(*mLua, LUA_REGISTRYINDEX, mRef);
  }

  void PushValueToStack() {
    if (mRef == LUA_NOREF) {
      throw LuaError("Invalid reference");
    }
    lua_rawgeti(*mLua, LUA_REGISTRYINDEX, mRef);
  }

  std::shared_ptr<LuaStateImpl> GetLua() const noexcept {
    return mLua;
  }

  LuaType GetType() const noexcept {
    return mType;
  }

 private:
  std::shared_ptr<LuaStateImpl> mLua;
  int mRef = LUA_NOREF;
  LuaType mType = LuaType::TNone;
};

class LuaStackCheck final {
 public:
  LuaStackCheck() = delete;
  LuaStackCheck(const std::shared_ptr<LuaStateImpl>& lua) : mLua(lua) {
    if (!lua) {
      return;
    }
    mStackSize = lua_gettop(*lua);
  }

  ~LuaStackCheck() noexcept {
    if (!mLua) {
      return;
    }

    const auto newSize = lua_gettop(*mLua);
    if (mStackSize != newSize) {
      dprintf("Lua stack size changed from {} to {}", mStackSize, newSize);
      OPENKNEEBOARD_BREAK;
      std::terminate();
    }
  }

 private:
  std::shared_ptr<LuaStateImpl> mLua;
  int mStackSize;
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

using LuaRefImpl = detail::LuaRefImpl;
using LuaStateImpl = detail::LuaStateImpl;
using LuaStackCheck = detail::LuaStackCheck;

LuaState::LuaState() {
  mLua = std::make_shared<LuaStateImpl>();
}

LuaState::~LuaState() = default;

LuaState::operator lua_State*() const noexcept {
  if (!mLua) {
    return nullptr;
  }
  return *mLua;
}

void LuaState::DoFile(const std::filesystem::path& path) {
  const auto error = luaL_dofile(*mLua, to_utf8(path).c_str());
  if (error) {
    throw LuaError(std::format(
      "Failed to load lua file '{}': {}",
      to_utf8(path),
      lua_tostring(*mLua, -1)));
  }
}

LuaRef LuaState::GetGlobal(const char* name) const {
  lua_getglobal(*mLua, name);
  return {mLua};
}

LuaRef::LuaRef(const std::shared_ptr<LuaStateImpl>& lua) {
  p = std::make_shared<LuaRefImpl>(lua);
}

LuaRef::~LuaRef() = default;

LuaType LuaRef::GetType() const noexcept {
  if (!p) {
    return LuaType::TNone;
  }
  return p->GetType();
}

template <>
std::string LuaRef::Get<std::string>() const {
  if (!p) {
    throw LuaTypeError("Tried to get an invalid ref");
  }
  auto lua = p->GetLua();
  const LuaStackCheck stackCheck(lua);

  if (p->GetType() != LuaType::TString) {
    throw LuaTypeError(std::format(
      "A string was requested, but the value is a {}",
      lua_typename(*lua, static_cast<int>(p->GetType()))));
  }

  p->PushValueToStack();

  size_t length;
  const auto buffer = lua_tolstring(*lua, -1, &length);
  lua_pop(*lua, 1);

  return std::string {buffer, length};
}

template <>
double LuaRef::Get<double>() const {
  if (!p) {
    throw LuaTypeError("Tried to get an invalid ref");
  }
  auto lua = p->GetLua();
  const LuaStackCheck stackCheck(lua);

  if (p->GetType() != LuaType::TNumber) {
    throw LuaTypeError(std::format(
      "A doublewas requested, but the value is a {}",
      lua_typename(*lua, static_cast<int>(p->GetType()))));
  }

  p->PushValueToStack();
  const double value = lua_tonumber(*lua, -1);
  lua_pop(*lua, 1);

  return value;
}

LuaRef LuaRef::at(const char* wantedKey) const {
  if (!p) {
    throw LuaTypeError("Tried to index an invalid ref");
  }
  auto lua = p->GetLua();
  const LuaStackCheck stackCheck(lua);

  if (p->GetType() != LuaType::TTable) {
    throw LuaTypeError(std::format(
      "Attempted to index a {} as if it were a table",
      lua_typename(*lua, static_cast<int>(p->GetType()))));
  }

  for (const auto& [key, value]: *this) {
    if (key == wantedKey) {
      return value;
    }
  }

  throw LuaIndexError(
    std::format("Index '{}' does not exist in table", wantedKey));
}

LuaRef LuaRef::at(const LuaRef& key) const {
  if (!p) {
    throw LuaTypeError("Tried to index an invalid ref");
  }
  const LuaStackCheck stackCheck(p->GetLua());
  switch (key.GetType()) {
    case LuaType::TString: {
      const auto strKey = key.Get<std::string>();
      return at(strKey.c_str());
    }
    case LuaType::TNumber:
      // TODO
      [[fallthrough]];
    default:
      throw LuaTypeError(std::format(
        "Don't know how to use a {} as a key",
        lua_typename(*p->GetLua(), static_cast<int>(key.GetType()))));
  }
}

bool LuaRef::operator==(const LuaRef& other) const noexcept {
  if (this->GetType() != other.GetType()) {
    return false;
  }

  if (!(p && other.p)) {
    return true;
  }

  auto lua = p->GetLua();
  const LuaStackCheck stackCheck(lua);

  p->PushValueToStack();
  other.p->PushValueToStack();
  scope_guard popValues([&]() { lua_pop(*lua, 2); });

  return lua_rawequal(*lua, -1, -2);
}

bool LuaRef::operator==(const char* value) const noexcept {
  if (GetType() != LuaType::TString) {
    return false;
  }
  return Get<std::string>() == value;
}

bool LuaRef::operator==(std::string_view value) const noexcept {
  if (GetType() != LuaType::TString) {
    return false;
  }
  return Get<std::string>() == value;
}

LuaRef::const_iterator LuaRef::begin() const noexcept {
  return const_iterator(p);
}

LuaRef::const_iterator LuaRef::end() const noexcept {
  return const_iterator();
}

LuaRef::const_iterator::const_iterator() = default;

LuaRef::const_iterator::const_iterator(
  const std::shared_ptr<detail::LuaRefImpl>& table) {
  if (!table) {
    // may be end() iterator
    return;
  }
  auto lua = table->GetLua();
  if (table->GetType() != LuaType::TTable) {
    throw LuaTypeError(std::format(
      "Can't iterate a {}",
      lua_typename(*lua, static_cast<int>(table->GetType()))));
  }

  mTable = table;
  this->operator++();
}

LuaRef::const_iterator::~const_iterator() = default;

LuaRef::const_iterator::operator bool() const noexcept {
  return mTable && mKey.GetType() != LuaType::TNone
    && mValue.GetType() != LuaType::TNone;
}

std::pair<LuaRef, LuaRef> LuaRef::const_iterator::operator*() const {
  if (mKey.GetType() == LuaType::TNone || mValue.GetType() == LuaType::TNone) {
    throw std::logic_error("Dereferencing iterator past end");
  }
  return {mKey, mValue};
}

LuaRef::const_iterator& LuaRef::const_iterator::operator++() {
  if (!mTable) {
    throw std::logic_error("Iterating past the end");
  }
  auto lua = mTable->GetLua();
  const LuaStackCheck stackCheck(lua);

  mTable->PushValueToStack();

  if (mKey.GetType() == LuaType::TNone) {
    lua_pushnil(*lua);
  } else {
    mKey.p->PushValueToStack();
  }

  if (lua_next(*lua, -2) == 0) {
    lua_pop(*lua, 1);// pop the table; key was already popped by lua_next
    mTable = {};
    mKey = {};
    mValue = {};
    return *this;
  }
  mValue = LuaRef(lua);
  mKey = LuaRef(lua);
  // pop table
  lua_pop(*lua, 1);

  return *this;
}

}// namespace OpenKneeboard
