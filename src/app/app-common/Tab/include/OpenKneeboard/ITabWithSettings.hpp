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
concept tab_with_deserializable_settings
  = std::derived_from<T, ITabWithSettings>
  && json_deserializable<typename T::Settings>;

template <tab_with_deserializable_settings T>
struct TabSettingsTraits<T> {
  using Settings = typename T::Settings;
};

}// namespace OpenKneeboard
