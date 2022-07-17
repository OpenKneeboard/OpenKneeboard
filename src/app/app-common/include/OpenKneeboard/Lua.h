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

#include <concepts>
#include <memory>
#include <shims/filesystem>
#include <stdexcept>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

/** Helpers for reading Lua data from C++.
 *
 * General usage:
 *
 * ```
 * LuaState luaState;
 * luaState.DoFile(std::filesystem::path { "foo.lua" });
 * auto myLuaRef = luaState.GetGlobal("BAR");
 * auto myStr = myVar.Get<std::string>();
 * auto myDouble = myVar.Get<double>();
 * auto myInt = myVar.Cast<double>();
 *
 * for (const auto& [key, value]: myVar) {
 *   // ...
 * }
 * auto nested = myVar["foo"]["bar"]["baz"];
 * // .at() and .contains() are supported too
 * ```
 *
 * Design goals:
 * - throw where possible
 * - call `std::terminate` when there are memory management errors
 * - do either as soon as possible. For example, `LuaRef::at()` and
 *   `LuaRef::operator[]` will throw if the key doesn't exist, instead
 *   of returning a null object
 * - allow implicit casting, but only to limited types
 * - throw if a `static_cast<>` would not be sufficient
 */

namespace OpenKneeboard {

namespace detail {
class LuaStateImpl;
class LuaRefImpl;
}// namespace detail

class LuaError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class LuaTypeError : public LuaError {
 public:
  using LuaError::LuaError;
};

class LuaIndexError : public LuaError {
 public:
  using LuaError::LuaError;
};

enum class LuaType : int {
  TNone = LUA_TNONE,
  TNil = LUA_TNIL,
  TBoolean = LUA_TBOOLEAN,
  TLightUserData = LUA_TLIGHTUSERDATA,
  TNumber = LUA_TNUMBER,
  TString = LUA_TSTRING,
  TTable = LUA_TTABLE,
  TFunction = LUA_TFUNCTION,
  TUserData = LUA_TUSERDATA,
  TThread = LUA_TTHREAD,
};

template <class T>
constexpr bool allow_implicit_conversion_from_LuaRef = false;

// Too unsafe, mixed meanings
template <>
constexpr bool allow_implicit_conversion_from_LuaRef<bool> = false;

template <>
constexpr bool allow_implicit_conversion_from_LuaRef<std::string> = true;
template <std::integral T>
constexpr bool allow_implicit_conversion_from_LuaRef<T> = true;
template <std::floating_point T>
constexpr bool allow_implicit_conversion_from_LuaRef<T> = true;

template <class T>
concept implicitly_convertible_from_LuaRef
  = allow_implicit_conversion_from_LuaRef<T>;

/** Reference to a Lua value.
 *
 * General usage:
 * - Get<T>(): get the value as a T, throwing an exception if the type
 *   doesn't exactly match
 * - Cast<T>(): get the value as a T, throwing an exception if the types
 *   are not `static_cast<>`able
 * - at(): index a table; throws if the ref isn't a table, or the key
 *   doesn't exist
 * - contains(): check if a key exists, or throw if the ref isn't a table
 * - operator[](): ditto
 * - begin(), end(): key-value iterators for tables
 */
class LuaRef final {
 public:
  class const_iterator;
  LuaRef() = default;
  /// Construct from stack
  LuaRef(const std::shared_ptr<detail::LuaStateImpl>&);
  ~LuaRef();

  bool operator==(const LuaRef&) const noexcept;
  template <class T>
    requires requires(const LuaRef& r) {
      r.Cast<T>();
    }
  bool operator==(const T& value) const noexcept {
    try {
      return this->Cast<T>() == value;
    } catch (const LuaTypeError&) {
      return false;
    }
  }
  bool operator==(const char* value) const noexcept;
  bool operator==(std::string_view value) const noexcept;

  LuaType GetType() const noexcept;

  // Tables
  LuaRef at(const char*) const;
  LuaRef at(const LuaRef&) const;
  template <class Key>
    requires requires(const LuaRef& r, const Key& k) {
      r == k;
    }
  bool contains(const Key& wantedKey) const {
    for (const auto& [key, value]: *this) {
      if (key == wantedKey) {
        return true;
      }
    }
    return false;
  }

  template <class T>
  LuaRef operator[](T&& key) const {
    return at(std::forward<T>(key));
  }

  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;

  // Explicit casts

  template <class T>
  T Get() const = delete;

  template <>
  std::string Get<std::string>() const;

  template <>
  double Get<double>() const;

  template <class T>
    requires requires(LuaRef x) {
      x.Get<T>();
    }
  T Cast() const {
    return Get<T>();
  }

  template <std::integral T>
  T Cast() const {
    return static_cast<T>(Get<double>());
  }
  template <std::floating_point T>
    requires(!std::same_as<double, T>)
  T Cast()
  const {
    return static_cast<T>(Get<double>());
  }

  // Implicit conversions
  template <implicitly_convertible_from_LuaRef T>
  operator T() const {
    return Cast<T>();
  }

  // **BONK** Go to C++ jail
  //
  // Multiple, contradictory expected meanings:
  // - is the ref valid?
  // - is the ref truthy if cast to a bool?
  operator bool() const = delete;

 private:
  std::shared_ptr<detail::LuaRefImpl> p;
};

class LuaState final {
 public:
  LuaState();
  ~LuaState();

  void DoFile(const std::filesystem::path&);

  LuaRef GetGlobal(const char* name) const;

  operator lua_State*() const noexcept;

 private:
  std::shared_ptr<detail::LuaStateImpl> mLua;
};

class LuaRef::const_iterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::pair<LuaRef, LuaRef>;
  using reference = const value_type&;

  const_iterator();
  const_iterator(const std::shared_ptr<detail::LuaRefImpl>& table);
  ~const_iterator();

  operator bool() const noexcept;
  value_type operator*() const;
  const_iterator& operator++();

  bool operator==(const const_iterator&) const noexcept = default;

 private:
  std::shared_ptr<detail::LuaRefImpl> mTable;
  LuaRef mKey;
  LuaRef mValue;
};

}// namespace OpenKneeboard
