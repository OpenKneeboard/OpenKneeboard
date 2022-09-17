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

#include <d2d1.h>

#include <cinttypes>
#include <map>
#include <memory>
#include <shims/filesystem>
#include <string>
#include <vector>

namespace OpenKneeboard::PDFNavigation {

struct Bookmark final {
  std::string mName;
  uint16_t mPageIndex;
};

enum class DestinationType {
  Page,
  URI,
};

struct Destination final {
  DestinationType mType;
  uint16_t mPageIndex = 0;
  std::string mURI;
  auto operator<=>(const Destination&) const = default;
};

struct Link final {
  D2D1_RECT_F mRect;
  Destination mDestination;

  constexpr bool operator==(const Link& other) const noexcept {
    return mDestination == other.mDestination
      && memcmp(&mRect, &other.mRect, sizeof(mRect)) == 0;
  }
};

class PDF final {
 public:
  PDF() = delete;
  PDF(const std::filesystem::path&);
  ~PDF();

  std::vector<Bookmark> GetBookmarks();
  std::vector<std::vector<Link>> GetLinks();

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard::PDFNavigation
