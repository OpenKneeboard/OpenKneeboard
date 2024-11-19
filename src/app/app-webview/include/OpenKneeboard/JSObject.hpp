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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#pragma once

#include "StringTemplateParameter.hpp"

#include <OpenKneeboard/json.hpp>

#include <FredEmmott/magic_json_serialize_enum.hpp>

#include <filesystem>
#include <ranges>
#include <unordered_map>

namespace OpenKneeboard {

struct JSNativeData {
  virtual ~JSNativeData() = default;
};

template <class T>
struct JSTypeInfo;

template <class TObj, auto TPropertyName, auto TGetter, auto TSetter>
struct JSProp {
  using value_type
    = std::decay_t<std::invoke_result_t<decltype(TGetter), TObj*>>;

  static constexpr std::string_view name_v {TPropertyName};

  static constexpr bool is_read_only_v
    = std::same_as<decltype(TSetter), std::nullptr_t>;

  static constexpr auto GetName() noexcept {
    return name_v;
  }

  static constexpr auto GetJSTypeName() noexcept {
    return JSTypeInfo<value_type>::js_typename_v;
  }

  [[nodiscard]] constexpr static value_type Get(const TObj& obj) noexcept {
    return std::invoke(TGetter, obj);
  }

  template <std::convertible_to<value_type> TNewValue>
    requires(!is_read_only_v)
  static void Set(TObj& obj, TNewValue&& value) noexcept {
    std::invoke(TSetter, obj, std::forward<TNewValue>(value));
  }
};

template <class TObj, auto TMethodName, auto TMethod>
struct JSMethod {
 private:
  template <class T>
  struct arg_types;

  template <class R, class... Args>
  struct arg_types<R (TObj::*)(Args...) const> {
    using type = std::tuple<Args...>;
  };

 public:
  static constexpr std::string_view name_v {TMethodName};
  using arguments_t = typename arg_types<decltype(TMethod)>::type;

  static constexpr auto GetName() noexcept {
    return name_v;
  }

  template <class T>
    requires std::same_as<TObj, std::decay_t<T>>
  static void Invoke(T&& obj, const nlohmann::json& argsArray) {
    static constexpr auto argCount = std::tuple_size_v<arguments_t>;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      std::invoke(TMethod, std::forward<T>(obj), argsArray.at(I)...);
    }(std::make_index_sequence<argCount>());
  }
};

template <class T>
struct JSClass;

template <class T>
concept has_js_methods = requires { JSClass<T>::methods_v; };

template <class T>
concept has_js_properties = requires { JSClass<T>::properties_v; };

template <class Derived, class T, auto TJSName, auto TCPPName>
struct JSClassImpl {
  using cpp_type_t = T;
  static constexpr auto js_type_name_v = TJSName;

  static constexpr auto GetJSTypeName() noexcept {
    return std::string_view {TJSName};
  }

  static constexpr auto GetCPPTypeName() noexcept {
    return std::string_view {TCPPName};
  }

  static constexpr auto GetArgumentType() noexcept {
    return GetJSTypeName();
  }

  template <class TFn>
  [[nodiscard]]
  static constexpr auto MapProperties(TFn&& fn) {
    return MapTuple(Derived::properties_v, std::forward<TFn>(fn));
  }

  static void SetPropertyByName(auto& native, auto propertyName, auto value) {
    const auto visitor = [&](auto prop) constexpr -> IterationResult {
      using enum IterationResult;
      if (propertyName != prop.GetName()) {
        return Continue;
      }
      if constexpr (prop.is_read_only_v) {
        return Continue;
      } else {
        prop.Set(native, value);
        return Break;
      }
    };

    IterateTuple(Derived::properties_v, visitor);
  }

  static void InvokeMethodByName(
    auto& native,
    auto name,
    const nlohmann::json& argumentsArray)
    requires has_js_methods<T>
  {
    const auto visitor = [&](auto method) constexpr -> IterationResult {
      using enum IterationResult;
      if (name != method.GetName()) {
        return Continue;
      }
      method.Invoke(native, argumentsArray);
      return Break;
    };

    IterateTuple(Derived::methods_v, visitor);
  }

  template <class TFn>
  [[nodiscard]]
  static constexpr auto MapMethods(TFn&& fn)
    requires has_js_methods<T>
  {
    return MapTuple(Derived::methods_v, std::forward<TFn>(fn));
  }

 private:
  enum class IterationResult {
    Continue,
    Break,
  };

  template <class TFn>
  [[nodiscard]]
  static constexpr auto IterateTuple(auto container, TFn&& fn) {
    return []<size_t... I>(
             auto container, auto fn, std::index_sequence<I...>) constexpr {
      using enum IterationResult;
      ((std::invoke(fn, std::get<I>(container)) == Break) || ...);
    }(container,
           std::forward<TFn>(fn),
           std::make_index_sequence<std::tuple_size_v<decltype(container)>> {});
  }

  template <class TFn>
  [[nodiscard]]
  static constexpr auto MapTuple(auto container, TFn&& fn) {
    return []<size_t... I>(
             auto container, auto fn, std::index_sequence<I...>) constexpr {
      return std::array {std::invoke(fn, std::get<I>(container))...};
    }(container,
           std::forward<TFn>(fn),
           std::make_index_sequence<std::tuple_size_v<decltype(container)>> {});
  }
};

template <class T>
struct JSEnum;

template <class Derived, class T, auto TJSName>
struct JSEnumImpl {
  using cpp_type_t = T;
  static constexpr auto enum_name_v = TJSName;

  static constexpr std::string_view GetJSTypeName() noexcept {
    return TJSName;
  }

  static constexpr std::string_view GetArgumentType() noexcept {
    return argument_type_v;
  }

 private:
  // We need some (compile-time) storage so we don't return an
  // `std::string_view` referring to a temporary, even if it's a compile-time
  // temporary.
  static constexpr auto argument_type_v = "keyof typeof "_tp + TJSName;
};

namespace detail {
template <class... T>
constexpr auto tuple_drop_back_fn(std::tuple<T...> v) {
  return []<size_t... I>(auto tuple, std::index_sequence<I...>) constexpr {
    return std::tuple {std::get<I>(tuple)...};
  }(v, std::make_index_sequence<sizeof...(T) - 1> {});
}

template <class T>
using tuple_drop_back_t = decltype(tuple_drop_back_fn(std::declval<T>()));
};// namespace detail

#define DECLARE_JS_CLASS(JS_CLASS_NAME) \
  template <> \
  struct JSClass<JS_CLASS_NAME> : JSClassImpl< \
                                    JSClass<JS_CLASS_NAME>, \
                                    JS_CLASS_NAME, \
                                    #JS_CLASS_NAME "Native"_tp, \
                                    #JS_CLASS_NAME ""_tp>

#define DECLARE_JS_PROPERTY(JS_PROP_NAME) \
  JSProp< \
    cpp_type_t, \
    #JS_PROP_NAME ""_tp, \
    &cpp_type_t::Get##JS_PROP_NAME, \
    &cpp_type_t::Set##JS_PROP_NAME>()

#define DECLARE_READ_ONLY_JS_PROPERTY(JS_PROP_NAME) \
  JSProp< \
    cpp_type_t, \
    #JS_PROP_NAME ""_tp, \
    &cpp_type_t::Get##JS_PROP_NAME, \
    nullptr>()

#define DECLARE_JS_PROPERTIES static constexpr auto properties_v = std::tuple

#define DECLARE_JS_METHOD(JS_METHOD_NAME) \
  JSMethod<cpp_type_t, #JS_METHOD_NAME ""_tp, &cpp_type_t::JS_METHOD_NAME>()
#define DECLARE_JS_METHODS static constexpr auto methods_v = std::tuple

#define DECLARE_JS_NAMED_ENUM(JS_ENUM_NAME, CPP_TYPE) \
  template <> \
  struct JSEnum<CPP_TYPE> \
    : JSEnumImpl<JSEnum<CPP_TYPE>, CPP_TYPE, JS_ENUM_NAME> {}; \
  FREDEMMOTT_MAGIC_JSON_SERIALIZE_ENUM(CPP_TYPE);

#define DECLARE_JS_MEMBER_ENUM(CPP_CLASS, ENUM_NAME) \
  DECLARE_JS_NAMED_ENUM( \
    #CPP_CLASS "Native_" #ENUM_NAME ""_tp, CPP_CLASS::ENUM_NAME);

#define DECLARE_JS_STRUCT_FIELD(JS_FIELD_NAME) \
  JSProp< \
    cpp_type_t, \
    (#JS_FIELD_NAME ""_tp).remove_prefix("m"_tp), \
    [](const cpp_type_t* o) { return o->JS_FIELD_NAME; }, \
    [](cpp_type_t* o, const decltype(o->JS_FIELD_NAME)& value) { \
      o->JS_FIELD_NAME = value; \
    }>(),

#define DECLARE_JS_NAMED_STRUCT(JS_STRUCT_NAME, CPP_TYPE, ...) \
  template <> \
  struct JSClass<CPP_TYPE> : JSClassImpl< \
                               JSClass<CPP_TYPE>, \
                               CPP_TYPE, \
                               JS_STRUCT_NAME, \
                               JS_STRUCT_NAME> { \
    DECLARE_JS_PROPERTIES {NLOHMANN_JSON_EXPAND( \
      NLOHMANN_JSON_PASTE(DECLARE_JS_STRUCT_FIELD, __VA_ARGS__))}; \
  };

#define DECLARE_JS_STRUCT_MEMBER_STRUCT(OUTER_STRUCT, INNER_STRUCT, ...) \
  DECLARE_JS_NAMED_STRUCT( \
    #OUTER_STRUCT "_" #INNER_STRUCT ""_tp, \
    OUTER_STRUCT::INNER_STRUCT, \
    __VA_ARGS__)

}// namespace OpenKneeboard