// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include "viewer-settings.hpp"

#include <OpenKneeboard/Filesystem.hpp>

#include <OpenKneeboard/json.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <algorithm>
#include <format>
#include <fstream>

namespace OpenKneeboard {
ViewerSettings ViewerSettings::Load() {
  ViewerSettings ret;

  const auto path = Filesystem::GetSettingsDirectory() / "Viewer.json";
  if (std::filesystem::exists(path)) {
    std::ifstream f(path.c_str());
    nlohmann::json json;
    f >> json;
    ret = json;
  }

  return ret;
}

void ViewerSettings::Save() {
  const auto dir = Filesystem::GetSettingsDirectory();
  if (!std::filesystem::exists(dir)) {
    std::filesystem::create_directories(dir);
  }

  nlohmann::json j;
  const auto path = Filesystem::GetSettingsDirectory() / "Viewer.json";
  if (std::filesystem::exists(path)) {
    std::ifstream f(path.c_str());
    f >> j;
  }

  to_json_with_default(j, ViewerSettings {}, *this);

  std::ofstream f(dir / "Viewer.json");
  f << std::setw(2) << j << std::endl;
}

#define IT(x) {ViewerAlignment::x, #x},
NLOHMANN_JSON_SERIALIZE_ENUM(ViewerAlignment, {OPENKNEEBOARD_VIEWER_ALIGNMENTS})
#undef IT

NLOHMANN_JSON_SERIALIZE_ENUM(
  ViewerFillMode,
  {
    {ViewerFillMode::Default, "Default"},
    {ViewerFillMode::Checkerboard, "Checkerboard"},
    {ViewerFillMode::Transparent, "Transparent"},
  })

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  ViewerSettings,
  mWindowWidth,
  mWindowHeight,
  mWindowX,
  mWindowY,
  mFillMode,
  mBorderless,
  mStreamerMode,
  mAlignment)

}// namespace OpenKneeboard
