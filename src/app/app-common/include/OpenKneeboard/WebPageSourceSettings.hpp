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

#include <OpenKneeboard/Pixels.hpp>

#include <OpenKneeboard/config.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace OpenKneeboard {

struct WebPageSourceSettings {
  PixelSize mInitialSize {Config::DefaultWebPagePixelSize};
  bool mExposeOpenKneeboardAPIs {true};
  bool mIntegrateWithSimHub {true};
  std::string mURI;
  bool mTransparentBackground {true};

  ///// NOT SAVED - JUST FOR INTERNAL USE (e.g. PluginTab) /////
  std::unordered_map<std::string, std::filesystem::path> mVirtualHosts;

  constexpr bool operator==(const WebPageSourceSettings&) const noexcept
    = default;
};
}// namespace OpenKneeboard