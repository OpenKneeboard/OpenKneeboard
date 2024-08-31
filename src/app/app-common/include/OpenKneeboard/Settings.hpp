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

#include <OpenKneeboard/AppSettings.hpp>
#include <OpenKneeboard/DirectInputSettings.hpp>
#include <OpenKneeboard/DoodleSettings.hpp>
#include <OpenKneeboard/LegacyNonVRSettings.hpp>
#include <OpenKneeboard/TabletSettings.hpp>
#include <OpenKneeboard/TextSettings.hpp>
#include <OpenKneeboard/UISettings.hpp>
#include <OpenKneeboard/VRSettings.hpp>
#include <OpenKneeboard/ViewsSettings.hpp>

#include <filesystem>

namespace OpenKneeboard {

#define OPENKNEEBOARD_GLOBAL_SETTINGS_SECTIONS IT(AppSettings, App)

#define OPENKNEEBOARD_PER_PROFILE_SETTINGS_SECTIONS \
  IT(DirectInputSettings, DirectInput) \
  IT(DoodleSettings, Doodles) \
  IT(TextSettings, Text) \
  IT(nlohmann::json, Games) \
  IT(TabletSettings, TabletInput) \
  IT(nlohmann::json, Tabs) \
  IT(UISettings, UI) \
  IT(ViewsSettings, Views) \
  IT(VRSettings, VR)

#define OPENKNEEBOARD_SETTINGS_SECTIONS \
  OPENKNEEBOARD_GLOBAL_SETTINGS_SECTIONS \
  OPENKNEEBOARD_PER_PROFILE_SETTINGS_SECTIONS

struct Settings final {
#define IT(cpptype, name) cpptype m##name {};
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT
  LegacyNonVRSettings mDeprecatedNonVR {};

  static Settings Load(winrt::guid defaultProfile, winrt::guid activeProfile);
  void Save(winrt::guid defaultProfile, winrt::guid activeProfile) const;
#define IT(cpptype, name) \
  void Reset##name##Section( \
    winrt::guid defaultProfile, winrt::guid activeProfile);
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

  bool operator==(const Settings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(Settings);

}// namespace OpenKneeboard
