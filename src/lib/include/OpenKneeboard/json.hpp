// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/json_fwd.hpp>

#include <shims/nlohmann/json.hpp>

namespace OpenKneeboard {

/* Okay, so this file is every kind of fun: templates, consteval, and macros :D
 *
 * The intent is to provide an alternative to
 * NLOHMANN_JSON_DEFINE_TYPE_NONINTRUSIVE() that:
 *
 * - only writes JSON for settings that differ from a 'default'; this can either
 *   be a default-constructed object, or, for example, a parent profile,
 *   allowing inheritance.
 * - normalizes written keys to the form `UpperCamelCase`
 * - supports reading either in that form or `lowerCamelCase` for compatibility
 * - supports the convention of `mUpperCamelCase` for the C++ structs
 *
 * concepts
 * ========
 *
 * - `json_deserializable<T>`: can be converted to JSON via `from_json()`
 * - `json_serializable<T>`: can be converted to JSON via `to_json()`
 * - `json_serdes<T>`: both of the above
 *
 * Macros
 * ======
 *
 * `OPENKNEEBOARD_DEFINE_JSON(T, ...)`
 * -----------------------------------
 *
 * Conceptually similar to `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE()`, however:
 * - can compile-time transform keys, e.g. `mFoo` -> `foo`
 * - ignores empty keys when reading
 *
 * The variadic arguments are the struct members.

 * `OPENKNEEBOARD_DEFINE_SPARSE_JSON(T, ...)`
 * ------------------------------------------
 *
 * Similar to `OPENKNEEBOARD_DEFINE_JSON`, but it skips keys that match the
 * 'default' object, unless they are already present in the JSON.
 * - for `to_json()`, this is default-constructed
 * - it also adds `to_json_with_default()` taking an explicit default/parent
 *     object
 *
 * The variadic arguments are the struct members.
 *
 *
 * `OPENKNEEBOARD_DECLARE_SPARSE_JSON(T)`
 * --------------------------------------
 *
 * From `<OpenKneeboard/json_fwd.hpp>`, optional forward declarations for
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

template <class T>
concept json_serializable = requires(nlohmann::json& j, const T& v) {
  { to_json(j, v) };
};
template <class T>
concept json_deserializable = requires(const nlohmann::json& j, T& v) {
  { from_json(j, v) };
};
template <class T>
concept json_serdes = json_serializable<T> && json_deserializable<T>;

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
  static void Encode(nlohmann::json& j, const T& /* parent_v */, const T& v) {
    j = v;
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
void to_json_postprocess(nlohmann::json&, const T&) {
}

template <class T>
void to_json_postprocess(nlohmann::json& j, const T& /*parent_v*/, const T& v) {
  to_json_postprocess<T>(j, v);
}

template <class T>
void from_json_postprocess(const nlohmann::json&, T&) {
}

#define DETAIL_OPENKNEEBOARD_FROM_JSON_DECLARE_KEY(KeyTransform) \
  constexpr auto ok_json_field_key_##KeyTransform \
    = detail::SparseJson::ConstStr##KeyTransform(wrapped);
#define DETAIL_OPENKNEEBOARD_FROM_JSON_TRY_KEY(v1, KeyTransform) \
  if (nlohmann_json_j.contains(ok_json_field_key_##KeyTransform)) { \
    nlohmann_json_v.v1 = nlohmann_json_j.at(ok_json_field_key_##KeyTransform) \
                           .get<decltype(nlohmann_json_v.v1)>(); \
  }
#define DETAIL_OPENKNEEBOARD_FROM_JSON(v1) \
  { \
    constexpr auto wrapped = detail::SparseJson::Wrap(#v1); \
    DETAIL_OPENKNEEBOARD_FROM_JSON_DECLARE_KEY(SkipFirst) \
    DETAIL_OPENKNEEBOARD_FROM_JSON_DECLARE_KEY(SkipFirstLowerNext) \
    DETAIL_OPENKNEEBOARD_FROM_JSON_TRY_KEY(v1, SkipFirst) \
    else DETAIL_OPENKNEEBOARD_FROM_JSON_TRY_KEY(v1, SkipFirstLowerNext) \
  }

#define DETAIL_OPENKNEEBOARD_TO_SPARSE_JSON(v1) \
  constexpr auto ok_json_field_key_##v1 \
    = detail::SparseJson::ConstStrSkipFirst(detail::SparseJson::Wrap(#v1)); \
  const bool persist_##v1 = nlohmann_json_j.contains(ok_json_field_key_##v1) \
    || (nlohmann_json_default_object.v1 != nlohmann_json_v.v1); \
  if (persist_##v1) { \
    SparseJsonDetail<decltype(nlohmann_json_v.v1)>::Encode( \
      nlohmann_json_j[ok_json_field_key_##v1], \
      nlohmann_json_default_object.v1, \
      nlohmann_json_v.v1); \
  }

// On MSVC, `FOO(__VA_ARGS__)` will pass everything as a single argument,
// but `FOO(OPENKNEEBOARD_IDVA(__VA_ARGS__))` will pass as multiple
// arguments
#define OPENKNEEBOARD_IDVA(...) __VA_ARGS__

#define OPENKNEEBOARD_DEFINE_SPARSE_JSON_DETAILS(T) \
  OPENKNEEBOARD_DECLARE_SPARSE_JSON(T) \
  template <> \
  struct SparseJsonDetail<T> { \
    static inline void \
    Encode(nlohmann::json& j, const T& parent_v, const T& v) { \
      to_json_with_default(j, parent_v, v); \
    } \
  };

#define OPENKNEEBOARD_DEFINE_FROM_JSON(T, ...) \
  void from_json(const nlohmann::json& nlohmann_json_j, T& nlohmann_json_v) { \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE( \
      DETAIL_OPENKNEEBOARD_FROM_JSON, OPENKNEEBOARD_IDVA(__VA_ARGS__))) \
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

#define OPENKNEEBOARD_DEFINE_SPARSE_JSON(T, ...) \
  OPENKNEEBOARD_DEFINE_SPARSE_JSON_DETAILS(T) \
  OPENKNEEBOARD_DEFINE_FROM_JSON(T, OPENKNEEBOARD_IDVA(__VA_ARGS__)) \
  OPENKNEEBOARD_DEFINE_SPARSE_JSON_TO_JSON_WITH_DEFAULT( \
    T, OPENKNEEBOARD_IDVA(__VA_ARGS__)) \
  OPENKNEEBOARD_DEFINE_SPARSE_JSON_TO_JSON(T)

#define OPENKNEEBOARD_DEFINE_JSON_DETAILS(T) \
  OPENKNEEBOARD_DECLARE_JSON(T) \
  template <> \
  struct SparseJsonDetail<T> { \
    static inline void \
    Encode(nlohmann::json& j, const T& /*parent_v*/, const T& v) { \
      to_json(j, v); \
    } \
  };
#define DETAIL_OPENKNEEBOARD_TO_JSON(v1) \
  constexpr auto ok_json_field_key_##v1 \
    = detail::SparseJson::ConstStrSkipFirst(detail::SparseJson::Wrap(#v1)); \
  SparseJsonDetail<decltype(nlohmann_json_v.v1)>::Encode( \
    nlohmann_json_j[ok_json_field_key_##v1], {}, nlohmann_json_v.v1);
#define OPENKNEEBOARD_DEFINE_TO_JSON(T, ...) \
  void to_json(nlohmann::json& nlohmann_json_j, const T& nlohmann_json_v) { \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE( \
      DETAIL_OPENKNEEBOARD_TO_JSON, OPENKNEEBOARD_IDVA(__VA_ARGS__))) \
    to_json_postprocess<T>(nlohmann_json_j, nlohmann_json_v); \
  }
#define OPENKNEEBOARD_DEFINE_JSON(T, ...) \
  OPENKNEEBOARD_DEFINE_JSON_DETAILS(T) \
  OPENKNEEBOARD_DEFINE_FROM_JSON(T, OPENKNEEBOARD_IDVA(__VA_ARGS__)) \
  OPENKNEEBOARD_DEFINE_TO_JSON(T, OPENKNEEBOARD_IDVA(__VA_ARGS__))

}// namespace OpenKneeboard
