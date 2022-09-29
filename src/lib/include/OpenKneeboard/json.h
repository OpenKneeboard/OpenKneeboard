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

#include <OpenKneeboard/json_fwd.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

/* Okay, so this file is every kind of fun: templates, consteval, and macros :D
 *
 * Macros
 * ======
 *
 * `OPENKNEEBOARD_DEFINE_SPARSE_JSON(T, KeyTransform, ...)`
 * --------------------------------------------------------
 *
 * Conceptually similar to `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE()`, however:
 * - can compile-time transform keys, e.g. `mFoo` -> `foo`
 * - ignores empty keys when reading
 * - skips keys that match the 'default' object
 *   - for `to_json()`, this is default-constructed
 *   - it also adds `to_json_with_default()` taking an explicit default/parent
 *     object
 *
 * For `KeyTransform`, you can pass:
 * - `Verbatim`: no change
 * - `LowerFirst`: 'FooBar' => 'fooBar'
 * - `SkipFirst`: 'mFoo' => 'Foo'
 * - `SkipFirstLowerNext`: 'mFooBar' => 'fooBar'
 *
 * The variadic arguments are the struct members.
 *
 * `OPENKNEEBOARD_DECLARE_SPARSE_JSON(T)`
 * --------------------------------------
 *
 * From `<OpenKneeboard/json_fwd.h>`, optional forward declarations for
 * `OPENKNEEBOARD_DEFINE_SPARSE_JSON`
 *
 * Function Templates
 * ==================
 *
 * `to_json_postprocess<T>(...)` and `from_json_postprocess<T>(...)`
 * -----------------------------------------------------------------
 *
 * Specialize these functions to add special logic to the macro-defined
 * functions for your types, e.g.:
 *
 * - serialize/deserialize some fields in a custom way
 * - add backwards-compatibility handling
 *
 * These functions will be called at the end of the generated functions.
 */

namespace detail::SparseJson {
// C++ currently bans directly returning arrays
template <std::size_t Len>
struct StringWrapper {
  char mBuffer[Len] {};

  consteval auto operator[](std::size_t idx) const noexcept {
    return mBuffer[idx];
  }

  operator std::string_view() const noexcept {
    return std::string_view {mBuffer, Len - 1};
  }

  operator std::string() const noexcept {
    return {mBuffer, Len - 1};
  }

  static consteval StringWrapper Create(const char (&str)[Len]) {
    StringWrapper<Len> ret;
    std::copy_n(str, Len, ret.mBuffer);
    return ret;
  }
};

template <std::size_t Len>
consteval auto Wrap(const char (&str)[Len]) {
  return StringWrapper<Len>::Create(str);
}

}// namespace detail::SparseJson

template <class T>
struct SparseJsonDetail {
  static inline void Encode(nlohmann::json& j, const T& parent_v, const T& v) {
    j = v;
  }

  template <std::size_t Len>
  static consteval auto TransformKey(const char (&str)[Len]) {
    return detail::SparseJson::StringWrapper<Len>::Create(str);
  }
};

template <class T>
void to_json_with_default(nlohmann::json& j, const T& parent_v, const T& v) {
  return SparseJsonDetail<T>::Encode(j, parent_v, v);
}

namespace detail::SparseJson {
consteval char ConstCharToLower(const char c) {
  return (c >= 'A' && c <= 'Z') ? (c + 'a' - 'A') : c;
}

template <std::size_t Len>
consteval StringWrapper<Len> ConstStrVerbatim(StringWrapper<Len> str) {
  return str;
}

template <std::size_t Len, size_t... Offset>
consteval StringWrapper<Len> ConstStrLowerFirstImpl(
  StringWrapper<Len> str,
  std::integer_sequence<size_t, Offset...>) {
  return {.mBuffer {ConstCharToLower(str[0]), str[Offset + 1]...}};
}

template <std::size_t Len>
consteval StringWrapper<Len> ConstStrLowerFirst(StringWrapper<Len> str) {
  return ConstStrLowerFirstImpl(
    str, std::make_integer_sequence<size_t, Len - 1>());
}

template <std::size_t Len, std::size_t... Offset>
consteval StringWrapper<Len - 1> ConstStrSkipFirstImpl(
  StringWrapper<Len> str,
  std::integer_sequence<size_t, Offset...>) {
  return {.mBuffer {str[Offset + 1]...}};
}

template <std::size_t Len>
consteval StringWrapper<Len - 1> ConstStrSkipFirst(StringWrapper<Len> str) {
  return ConstStrSkipFirstImpl(
    str, std::make_integer_sequence<size_t, Len - 1>());
}

template <std::size_t Len>
consteval StringWrapper<Len - 1> ConstStrSkipFirstLowerNext(
  StringWrapper<Len> str) {
  return ConstStrLowerFirst(ConstStrSkipFirst(str));
}

};// namespace detail::SparseJson

template <class T>
void to_json_postprocess(nlohmann::json& j, const T& parent_v, const T& v) {
}

template <class T>
void from_json_postprocess(const nlohmann::json& j, T& v) {
}

#define DETAIL_OPENKNEEBOARD_FROM_SPARSE_JSON(v1) \
  { \
    constexpr auto ok_json_field_key \
      = SparseJsonDetail<std::decay_t<decltype(nlohmann_json_v)>>::TransformKey(#v1); \
    if (nlohmann_json_j.contains(ok_json_field_key)) { \
      nlohmann_json_v.v1 = nlohmann_json_j.at(ok_json_field_key); \
    } \
  }

#define DETAIL_OPENKNEEBOARD_TO_SPARSE_JSON(v1) \
  if (nlohmann_json_default_object.v1 != nlohmann_json_v.v1) { \
    constexpr auto ok_json_field_key \
      = SparseJsonDetail<std::decay_t<decltype(nlohmann_json_v)>>::TransformKey(#v1); \
    SparseJsonDetail<decltype(nlohmann_json_v.v1)>::Encode( \
      nlohmann_json_j[ok_json_field_key], \
      nlohmann_json_default_object.v1, \
      nlohmann_json_v.v1); \
  }

// On MSVC, `FOO(__VA_ARGS__)` will pass everything as a single argument,
// but `FOO(OPENKNEEBOARD_IDVA(__VA_ARGS__))` will pass as multiple
// arguments
#define OPENKNEEBOARD_IDVA(...) __VA_ARGS__
#define OPENKNEEBOARD_DEFINE_SPARSE_JSON_DETAILS(T, KeyTransform) \
  OPENKNEEBOARD_DECLARE_SPARSE_JSON(T) \
  template <> \
  struct SparseJsonDetail<T> { \
    static inline void \
    Encode(nlohmann::json& j, const T& parent_v, const T& v) { \
      to_json_with_default(j, parent_v, v); \
    } \
    template <std::size_t Len> \
    static consteval auto TransformKey(const char (&str)[Len]) { \
      return detail::SparseJson::ConstStr##KeyTransform( \
        detail::SparseJson::StringWrapper<Len>::Create(str)); \
    } \
  };

#define OPENKNEEBOARD_DEFINE_SPARSE_JSON_FROM_JSON(T, ...) \
  void from_json(const nlohmann::json& nlohmann_json_j, T& nlohmann_json_v) { \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE( \
      DETAIL_OPENKNEEBOARD_FROM_SPARSE_JSON, OPENKNEEBOARD_IDVA(__VA_ARGS__))) \
    from_json_postprocess<T>(nlohmann_json_j, nlohmann_json_v); \
  }

#define OPENKNEEBOARD_DEFINE_SPARSE_JSON_TO_JSON_WITH_DEFAULT(T, ...) \
  template <> \
  void to_json_with_default<T>( \
    nlohmann::json & nlohmann_json_j, \
    const T& nlohmann_json_default_object, \
    const T& nlohmann_json_v) { \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE( \
      DETAIL_OPENKNEEBOARD_TO_SPARSE_JSON, OPENKNEEBOARD_IDVA(__VA_ARGS__))) \
    to_json_postprocess<T>( \
      nlohmann_json_j, nlohmann_json_default_object, nlohmann_json_v); \
  }

#define OPENKNEEBOARD_DEFINE_SPARSE_JSON_TO_JSON(T) \
  void to_json(nlohmann::json& nlohmann_json_j, const T& nlohmann_json_v) { \
    T nlohmann_json_default_object {}; \
    to_json_with_default( \
      nlohmann_json_j, nlohmann_json_default_object, nlohmann_json_v); \
  }

#define OPENKNEEBOARD_DEFINE_SPARSE_JSON(T, KeyTransform, ...) \
  OPENKNEEBOARD_DEFINE_SPARSE_JSON_DETAILS(T, KeyTransform) \
  OPENKNEEBOARD_DEFINE_SPARSE_JSON_FROM_JSON( \
    T, OPENKNEEBOARD_IDVA(__VA_ARGS__)) \
  OPENKNEEBOARD_DEFINE_SPARSE_JSON_TO_JSON_WITH_DEFAULT( \
    T, OPENKNEEBOARD_IDVA(__VA_ARGS__)) \
  OPENKNEEBOARD_DEFINE_SPARSE_JSON_TO_JSON(T)

}// namespace OpenKneeboard
