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

namespace OpenKneeboard::PDFIPC {

struct Bookmark final {
  std::string mName;
  uint16_t mPageIndex;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Bookmark, mName, mPageIndex);

enum class DestinationType {
  Page,
  URI,
};
NLOHMANN_JSON_SERIALIZE_ENUM(
  DestinationType,
  {
    {DestinationType::Page, "Page"},
    {DestinationType::URI, "URI"},
  });

struct Destination final {
  DestinationType mType;
  uint16_t mPageIndex = 0;
  std::string mURI;
  auto operator<=>(const Destination&) const = default;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Destination, mType, mPageIndex, mURI)

struct NormalizedRect final {
  float mLeft;
  float mTop;
  float mRight;
  float mBottom;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  NormalizedRect,
  mLeft,
  mTop,
  mRight,
  mBottom);

struct Link final {
  NormalizedRect mRect;
  Destination mDestination;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Link, mRect, mDestination);

struct Request final {
  std::string mPDFFilePath;
  std::string mOutputFilePath;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Request, mPDFFilePath, mOutputFilePath);

struct Response final {
  std::string mPDFFilePath;
  std::vector<Bookmark> mBookmarks;
  std::vector<std::vector<Link>> mLinksByPage;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  Response,
  mPDFFilePath,
  mBookmarks,
  mLinksByPage);

}// namespace OpenKneeboard::PDFIPC
