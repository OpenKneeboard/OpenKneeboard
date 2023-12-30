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

#include <OpenKneeboard/AppSettings.h>
#include <OpenKneeboard/DirectInputSettings.h>
#include <OpenKneeboard/DoodleSettings.h>
#include <OpenKneeboard/FlatConfig.h>
#include <OpenKneeboard/TabletSettings.h>
#include <OpenKneeboard/TextSettings.h>
#include <OpenKneeboard/VRConfig.h>

#include <shims/filesystem>

namespace OpenKneeboard {

#define OPENKNEEBOARD_SETTINGS_SECTIONS \
  IT(AppSettings, App) \
  IT(DirectInputSettings, DirectInput) \
  IT(DoodleSettings, Doodles) \
  IT(TextSettings, Text) \
  IT(nlohmann::json, Games) \
  IT(NonVRConstrainedPosition, NonVR) \
  IT(TabletSettings, TabletInput) \
  IT(nlohmann::json, Tabs) \
  IT(VRConfig, VR)

struct Settings final {
#define IT(cpptype, name) cpptype m##name {};
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

  static Settings Load(std::string_view profileID);
  void Save(std::string_view profileID) const;
#define IT(cpptype, name) void Reset##name##Section(std::string_view profileID);
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

  constexpr auto operator<=>(const Settings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(Settings);

}// namespace OpenKneeboard
