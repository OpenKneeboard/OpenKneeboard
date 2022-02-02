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

#include <OpenKneeboard/DCSAircraftTab.h>
#include <OpenKneeboard/DCSMissionTab.h>
#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/DCSTerrainTab.h>
#include <OpenKneeboard/FolderTab.h>
#include <shims/winrt.h>
#include <shims/wx.h>

#include <concepts>

class wxWindow;

#define CONFIGURABLE_TAB_TYPES IT(_("Folder"), Folder)

#define ZERO_CONFIG_TAB_TYPES \
  IT(_("DCS Aircraft Kneeboard"), DCSAircraft) \
  IT(_("DCS Mission Kneeboard"), DCSMission) \
  IT(_("DCS Radio Log"), DCSRadioLog) \
  IT(_("DCS Terrain Kneeboard"), DCSTerrain)

#define TAB_TYPES \
  CONFIGURABLE_TAB_TYPES \
  ZERO_CONFIG_TAB_TYPES

// If this fails, check that you included the header :)
#define IT(_, type) \
  static_assert( \
    std::derived_from<OpenKneeboard::type##Tab, OpenKneeboard::Tab>);
TAB_TYPES
#undef IT

#define IT(_, type) \
  static_assert(std::invocable< \
                decltype(OpenKneeboard::type##Tab::FromSettings), \
                std::string, \
                nlohmann::json>);
CONFIGURABLE_TAB_TYPES
#undef IT

namespace OpenKneeboard {

struct DXResources;

enum {
#define IT(_, key) TABTYPE_IDX_##key,
  TAB_TYPES
#undef IT
};

// clang-format off
template<class T>
concept tab_with_default_constructor =
  std::derived_from<T, Tab>
  && std::is_default_constructible_v<T>;

template<class T>
concept tab_with_dxr_constructor =
  std::derived_from<T, Tab>
  && std::is_constructible_v<T, const DXResources&>;

template<class T>
concept tab_instantiable_from_settings =
  std::derived_from<T, Tab>
  && requires (std::string title, nlohmann::json config) {
    { T::FromSettings(title, config) } -> std::same_as<std::shared_ptr<T>>;
  };

template<class T>
concept tab_with_interactive_creation =
  std::derived_from<T, Tab>
  && requires (wxWindow* parent) {
    { T::Create(parent) } -> std::same_as<std::shared_ptr<T>>;
  };

template<class T>
concept tab_with_settings =
  tab_instantiable_from_settings<T>
  && tab_with_interactive_creation<T>;
// clang-format on

#define IT(_, it) static_assert(tab_with_settings<it##Tab>);
CONFIGURABLE_TAB_TYPES
#undef IT

}// namespace OpenKneeboard
