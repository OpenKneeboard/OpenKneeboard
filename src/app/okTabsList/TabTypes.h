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

#define ZERO_CONFIG_TAB_TYPES \
  IT(_("DCS Aircraft Kneeboard"), DCSAircraft) \
  IT(_("DCS Mission Kneeboard"), DCSMission) \
  IT(_("DCS Radio Log"), DCSRadioLog) \
  IT(_("DCS Terrain Kneeboard"), DCSTerrain)

#define TAB_TYPES \
  IT(_("Folder"), Folder) \
  ZERO_CONFIG_TAB_TYPES

// If this fails, check that you included the header :)
#define IT(_, type) \
  static_assert( \
    std::derived_from<OpenKneeboard::type##Tab, OpenKneeboard::Tab>);
TAB_TYPES
#undef IT

namespace OpenKneeboard {

enum {
#define IT(_, key) TABTYPE_IDX_##key,
  TAB_TYPES
#undef IT
};

}// namespace OpenKneeboard
