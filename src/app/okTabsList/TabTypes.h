#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <concepts>

#include "OpenKneeboard/DCSAircraftTab.h"
#include "OpenKneeboard/DCSMissionTab.h"
#include "OpenKneeboard/DCSRadioLogTab.h"
#include "OpenKneeboard/DCSTerrainTab.h"
#include "OpenKneeboard/FolderTab.h"

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

enum {
#define IT(_, key) TABTYPE_IDX_##key,
  TAB_TYPES
#undef IT
};

// clang-format off
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

#define IT(_, it) \
  static_assert(tab_with_settings<it##Tab>);
CONFIGURABLE_TAB_TYPES
#undef IT

}// namespace OpenKneeboard
