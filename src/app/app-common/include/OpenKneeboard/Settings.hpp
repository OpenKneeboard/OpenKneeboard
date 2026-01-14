// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/AppSettings.hpp>
#include <OpenKneeboard/DirectInputSettings.hpp>
#include <OpenKneeboard/DoodleSettings.hpp>
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
