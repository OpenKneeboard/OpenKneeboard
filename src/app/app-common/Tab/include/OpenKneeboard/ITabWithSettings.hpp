// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "ITab.hpp"

#include <OpenKneeboard/json.hpp>

#include <concepts>

namespace OpenKneeboard {

class ITabWithSettings : public virtual ITab {
 public:
  virtual nlohmann::json GetSettings() const = 0;
};

template <class T>
struct TabSettingsTraits;

template <std::derived_from<ITabWithSettings> T>
struct TabSettingsTraits<T> {
  using Settings = nlohmann::json;
};

template <class T>
concept tab_with_deserializable_settings =
  std::derived_from<T, ITabWithSettings>
  && json_deserializable<typename T::Settings>;

template <tab_with_deserializable_settings T>
struct TabSettingsTraits<T> {
  using Settings = typename T::Settings;
};

}// namespace OpenKneeboard
