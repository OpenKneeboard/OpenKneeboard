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

#include <windows.h>

#include <nlohmann/json.hpp>
#include <optional>

namespace OpenKneeboard {

struct AppSettings final {
  std::optional<RECT> mWindowRect;
  bool mLoopPages = false;
  bool mLoopTabs = false;
  bool mDualKneeboards = false;

  struct {
    uint64_t mDisabledUntil = 0;
    std::string mSkipVersion;
    std::string mForceUpgradeTo;// for testing
  } mAutoUpdate;
};

void from_json(const nlohmann::json&, AppSettings&);
void to_json(nlohmann::json&, const AppSettings&);

}// namespace OpenKneeboard
