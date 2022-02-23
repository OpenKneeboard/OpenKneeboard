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
#include <OpenKneeboard/PDFTab.h>
#include <OpenKneeboard/TextFileTab.h>
#include <shims/winrt.h>

#include <concepts>

class wxWindow;

#define OPENKNEEBOARD_TAB_TYPES \
  IT(_("Folder"), Folder) \
  IT(_("PDF"), PDF) \
  IT(_("Text File"), TextFile) \
  IT(_("DCS Aircraft Kneeboard"), DCSAircraft) \
  IT(_("DCS Mission Kneeboard"), DCSMission) \
  IT(_("DCS Radio Log"), DCSRadioLog) \
  IT(_("DCS Terrain Kneeboard"), DCSTerrain)

// If this fails, check that you included the header :)
#define IT(_, type) \
  static_assert( \
    std::derived_from<OpenKneeboard::type##Tab, OpenKneeboard::Tab>);
OPENKNEEBOARD_TAB_TYPES
#undef IT

namespace OpenKneeboard {

enum {
#define IT(_, key) TABTYPE_IDX_##key,
  OPENKNEEBOARD_TAB_TYPES
#undef IT
};

struct DXResources;

// clang-format off
template<class T>
concept tab_with_interactive_creation =
  std::derived_from<T, Tab>
  && requires (wxWindow* parent, DXResources dxr) {
    { T::Create(parent, dxr) } -> std::same_as<std::shared_ptr<T>>;
  };
// clang-format on

/** Create a `shared_ptr<Tab>` with existing config */
template <std::derived_from<Tab> T>
std::shared_ptr<T> load_tab(
  const DXResources& dxr,
  const std::string& title,
  const nlohmann::json& config) {
  if constexpr (std::constructible_from<T, DXResources>) {
    return std::make_shared<T>(dxr);
  }

  if constexpr (std::constructible_from<T, DXResources, std::string>) {
    return std::make_shared<T>(dxr, title);
  }

  if constexpr (
    std::constructible_from<T, DXResources, std::string, nlohmann::json>) {
    return std::make_shared<T>(dxr, title, config);
  }
}

/** Create a `shared_ptr<Tab>`, prompting the user for config if needed */
template <std::derived_from<Tab> T>
std::shared_ptr<T> create_tab(wxWindow* parent, const DXResources& dxr) {
  if constexpr (std::constructible_from<T, DXResources>) {
    return std::make_shared<T>(dxr);
  }

  if constexpr (tab_with_interactive_creation<T>) {
    return T::Create(parent, dxr);
  }
}

}// namespace OpenKneeboard
