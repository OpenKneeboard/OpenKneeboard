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

#include <filesystem>

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
// Using json.hpp instead of json_fwd.hpp for the enum macro
#include <nlohmann/json.hpp>
#endif

namespace OpenKneeboard {

enum class OpenXRMode {
  Disabled,
  CurrentUser,
};

void SetOpenXRModeWithHelperProcess(OpenXRMode);
// void SetOpenXRMode(OpenXRMode, const std::filesystem::path& json);

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
NLOHMANN_JSON_SERIALIZE_ENUM(
  OpenXRMode,
  {
    {OpenXRMode::Disabled, "Disabled"},
    {OpenXRMode::CurrentUser, "EnabledForCurrentUser"},
  })
#endif

}// namespace OpenKneeboard
