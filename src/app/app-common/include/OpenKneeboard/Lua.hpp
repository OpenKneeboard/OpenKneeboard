// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <concepts>
#include <filesystem>
#include <memory>
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
 * - support `const`
 * - generally feel more 'C++-like' than 'lua-like'
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
struct LuaTypeTraits;// = delete

template <>
struct LuaTypeTraits<bool>;// = delete

template <>
struct LuaTypeTraits<std::string> {
  static std::string Get(detail::LuaRefImpl*);
};

template <>
struct LuaTypeTraits<lua_Integer> {
  static lua_Integer Get(detail::LuaRefImpl*);
};

template <>
struct LuaTypeTraits<lua_Number> {
  static lua_Number Get(detail::LuaRefImpl*);
};

template <std::integral T>
struct LuaTypeTraits<T> {
  static auto Get(detail::LuaRefImpl* p) {
    return static_cast<T>(LuaTypeTraits<lua_Integer>::Get(p));
  }
};

template <std::floating_point T>
struct LuaTypeTraits<T> {
  static auto Get(detail::LuaRefImpl* p) {
    return static_cast<T>(LuaTypeTraits<lua_Number>::Get(p));
  }
};

template <class T>
concept implicitly_convertible_from_LuaRef = requires(detail::LuaRefImpl* p) {
  { LuaTypeTraits<T>::Get(p) } -> std::same_as<T>;
};

static_assert(implicitly_convertible_from_LuaRef<std::string>);
static_assert(implicitly_convertible_from_LuaRef<lua_Integer>);
static_assert(implicitly_convertible_from_LuaRef<int8_t>);
static_assert(implicitly_convertible_from_LuaRef<int64_t>);
static_assert(!implicitly_convertible_from_LuaRef<bool>);

/** Reference to a Lua value.
 *
 * General usage:
 * - Get<T>(): get the value as a T, throwing an exception if the type
 *   doesn't exactly match
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

  template <implicitly_convertible_from_LuaRef T>
  bool operator==(const T& other) const noexcept {
    try {
      return p && LuaTypeTraits<T>::Get(*p) == other;
    } catch (const LuaTypeError&) {
      return false;
    }
  }

  bool operator==(const char* value) const noexcept {
    try {
      return p && LuaTypeTraits<std::string>::Get(p.get()) == value;
    } catch (const LuaTypeError&) {
      return false;
    }
  }

  bool operator==(std::string_view value) const noexcept {
    try {
      return p && LuaTypeTraits<std::string>::Get(p.get()) == value;
    } catch (const LuaTypeError&) {
      return false;
    }
  }

  LuaType GetType() const noexcept;

  // Tables
  LuaRef at(const char*) const;
  LuaRef at(const LuaRef&) const;
  template <class Key>
    requires requires(const LuaRef& r, const Key& k) { r == k; }
  bool contains(this const auto& self, const Key& wantedKey) {
    for (const auto& [key, value]: self) {
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

  template <implicitly_convertible_from_LuaRef T>
  T Get() const {
    return LuaTypeTraits<T>::Get(p.get());
  }

  template <implicitly_convertible_from_LuaRef T>
  operator T() const {
    return LuaTypeTraits<T>::Get(p.get());
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
