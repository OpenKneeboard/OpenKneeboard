// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/Lua.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/utf8.hpp>

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

  LuaRefImpl() = delete;
  LuaRefImpl(const LuaRefImpl&) = delete;
  LuaRefImpl(LuaRefImpl&&) = delete;
  LuaRefImpl& operator=(const LuaRefImpl&) = delete;
  LuaRefImpl& operator=(LuaRefImpl&&) = delete;

 private:
  std::shared_ptr<LuaStateImpl> mLua;
  int mRef = LUA_NOREF;
  LuaType mType = LuaType::TNone;
};

class LuaStackCheck final {
 public:
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
      dprint("Lua stack size changed from {} to {}", mStackSize, newSize);
      OPENKNEEBOARD_BREAK;
      std::terminate();
    }
  }

  LuaStackCheck() = delete;
  LuaStackCheck(const LuaStackCheck&) = delete;
  LuaStackCheck(LuaStackCheck&&) = delete;
  LuaStackCheck& operator=(const LuaStackCheck&) = delete;
  LuaStackCheck& operator=(LuaStackCheck&&) = delete;

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
  const LuaStackCheck stackCheck(mLua);

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

std::string LuaTypeTraits<std::string>::Get(detail::LuaRefImpl* p) {
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

  size_t length {};
  const auto buffer = lua_tolstring(*lua, -1, &length);
  const std::string ret {buffer, length};
  lua_pop(*lua, 1);
  return ret;
}

lua_Integer LuaTypeTraits<lua_Integer>::Get(detail::LuaRefImpl* p) {
  if (!p) {
    throw LuaTypeError("Tried to get an invalid ref");
  }
  auto lua = p->GetLua();
  const LuaStackCheck stackCheck(lua);

  if (p->GetType() != LuaType::TNumber) {
    throw LuaTypeError(std::format(
      "An integer was requested, but the value is a {}",
      lua_typename(*lua, static_cast<int>(p->GetType()))));
  }

  p->PushValueToStack();
  const auto ret = lua_tointeger(*lua, -1);
  lua_pop(*lua, 1);

  return ret;
}

lua_Number LuaTypeTraits<lua_Number>::Get(detail::LuaRefImpl* p) {
  if (!p) {
    throw LuaTypeError("Tried to get an invalid ref");
  }
  auto lua = p->GetLua();
  const LuaStackCheck stackCheck(lua);

  if (p->GetType() != LuaType::TNumber) {
    throw LuaTypeError(std::format(
      "A number was requested, but the value is a {}",
      lua_typename(*lua, static_cast<int>(p->GetType()))));
  }

  p->PushValueToStack();
  const auto ret = lua_tonumber(*lua, -1);
  lua_pop(*lua, 1);

  return ret;
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
  scope_exit popValues([&]() { lua_pop(*lua, 2); });

  return lua_rawequal(*lua, -1, -2);
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
