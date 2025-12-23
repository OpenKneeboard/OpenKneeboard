// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#pragma once

#include <OpenKneeboard/inttypes.hpp>

#include <cinttypes>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <d2d1.h>

namespace OpenKneeboard::PDFNavigation {

struct Bookmark final {
  std::string mName;
  PageIndex mPageIndex;
};

enum class DestinationType {
  Page,
  URI,
};

struct Destination final {
  DestinationType mType;
  PageIndex mPageIndex = 0;
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
  PDF(PDF&&);
  ~PDF();

  std::vector<Bookmark> GetBookmarks();
  std::vector<std::vector<Link>> GetLinks();

  PDF& operator=(PDF&&);

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard::PDFNavigation
