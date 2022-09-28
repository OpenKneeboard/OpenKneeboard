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

#include <OpenKneeboard/DoodleSettings.h>
#include <OpenKneeboard/FlatConfig.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

#define OPENKNEEBOARD_SETTINGS_SECTIONS \
  IT(nlohmann::json, DirectInput) \
  IT(nlohmann::json, TabletInput) \
  IT(nlohmann::json, Games) \
  IT(nlohmann::json, Tabs) \
  IT(FlatConfig, NonVR) \
  IT(nlohmann::json, VR) \
  IT(nlohmann::json, App) \
  IT(DoodleSettings, Doodle)

struct Settings final {
#define IT(cpptype, name) cpptype name;
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

  static Settings Load(std::string_view profile = {});
  void Save(std::string_view profile = {});

  auto operator<=>(const Settings&) const noexcept = default;
};

void from_json(const nlohmann::json&, Settings&);

}// namespace OpenKneeboard
