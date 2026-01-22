// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <shims/nlohmann/json_fwd.hpp>
#include <shims/winrt/base.h>

namespace OpenKneeboard {
template <class T>
void to_json_with_default(nlohmann::json& j, const T& parent_v, const T& v);
}// namespace OpenKneeboard

#define OPENKNEEBOARD_DECLARE_SPARSE_JSON(T) \
  void from_json(const nlohmann::json& nlohmann_json_j, T& nlohmann_json_v); \
  template <> \
  void to_json_with_default<T>( \
    nlohmann::json & nlohmann_json_j, \
    const T& nlohmann_json_default_object, \
    const T& nlohmann_json_v); \
  void to_json(nlohmann::json& nlohmann_json_j, const T& nlohmann_json_v);

#define OPENKNEEBOARD_DECLARE_JSON(T) \
  void from_json(const nlohmann::json& nlohmann_json_j, T& nlohmann_json_v); \
  void to_json(nlohmann::json& nlohmann_json_j, const T& nlohmann_json_v);

namespace nlohmann {
template <>
struct adl_serializer<winrt::guid> {
  static void to_json(json&, const winrt::guid&);
  static void from_json(const json&, winrt::guid&);
};

}// namespace nlohmann
