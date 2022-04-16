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

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

struct Settings final {
  uint32_t Version = 1;

  nlohmann::json DirectInputV2;
  nlohmann::json TabletInput;
  nlohmann::json Games;
  nlohmann::json Tabs;
  nlohmann::json NonVR;
  nlohmann::json VR;
  nlohmann::json App;
  nlohmann::json Doodle;

  static Settings Load();
  void Save();
};

void from_json(const nlohmann::json&, Settings&);
void to_json(nlohmann::json&, const Settings&);

}// namespace OpenKneeboard
