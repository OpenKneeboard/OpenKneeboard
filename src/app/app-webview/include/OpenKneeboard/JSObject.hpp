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

#include <filesystem>
#include <ranges>
#include <unordered_map>

namespace OpenKneeboard {

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

  static constexpr auto GetTypeName() noexcept {
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
  static constexpr std::string_view name_v {TMethodName};

  static constexpr auto GetName() noexcept {
    return name_v;
  }

  template <class T>
    requires std::same_as<TObj, std::decay_t<T>>
    && std::invocable<decltype(TMethod), T>
  static void Invoke(T&& obj) {
    std::invoke(TMethod, std::forward<T>(obj));
  }
};

struct AnyJSClass {
  virtual ~AnyJSClass() = default;
};

template <class Derived, class T, StringTemplateParameter TName>
struct JSClassImpl : AnyJSClass {
  using cpp_type_t = T;
  static constexpr auto class_name_v = TName;

  static constexpr auto GetTypeName() noexcept {
    return std::string_view {TName};
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

  static void InvokeMethodByName(auto& native, auto name) {
    const auto visitor = [&](auto method) constexpr -> IterationResult {
      using enum IterationResult;
      if (name != method.GetName()) {
        return Continue;
      }
      method.Invoke(native);
      return Break;
    };

    IterateTuple(Derived::methods_v, visitor);
  }

  template <class TFn>
  [[nodiscard]]
  static constexpr auto MapMethods(TFn&& fn) {
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
struct JSClass;

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
                                    #JS_CLASS_NAME "Native">

#define DECLARE_JS_PROPERTY(JS_PROP_NAME) \
  JSProp< \
    cpp_type_t, \
    StringTemplateParameter {#JS_PROP_NAME}, \
    &cpp_type_t::Get##JS_PROP_NAME, \
    &cpp_type_t::Set##JS_PROP_NAME>()

#define DECLARE_READ_ONLY_JS_PROPERTY(JS_PROP_NAME) \
  JSProp< \
    cpp_type_t, \
    StringTemplateParameter {#JS_PROP_NAME}, \
    &cpp_type_t::Get##JS_PROP_NAME, \
    nullptr>()

#define DECLARE_JS_PROPERTIES static constexpr auto properties_v = std::tuple

#define DECLARE_JS_METHOD(JS_METHOD_NAME) \
  JSMethod< \
    cpp_type_t, \
    StringTemplateParameter {#JS_METHOD_NAME}, \
    &cpp_type_t::JS_METHOD_NAME>()
#define DECLARE_JS_METHODS static constexpr auto methods_v = std::tuple

}// namespace OpenKneeboard