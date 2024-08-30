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
#include <OpenKneeboard/LegacyNonVRSettings.hpp>

#include <OpenKneeboard/json/LegacyNonVRSettings.hpp>
#include <OpenKneeboard/json/NonVRConstrainedPosition.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

void to_json(nlohmann::json& j, const LegacyNonVRSettings& v) {
  to_json(j, static_cast<const NonVRConstrainedPosition&>(v));
  j["Opacity"] = v.mOpacity;
}
void from_json(const nlohmann::json& j, LegacyNonVRSettings& v) {
  from_json(j, static_cast<NonVRConstrainedPosition&>(v));
  if (j.contains("Opacity")) {
    v.mOpacity = j.at("Opacity");
  }
}

}// namespace OpenKneeboard
