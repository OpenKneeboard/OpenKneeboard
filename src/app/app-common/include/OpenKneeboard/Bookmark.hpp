// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ITab.hpp>

#include <OpenKneeboard/inttypes.hpp>

#include <memory>
#include <string>

namespace OpenKneeboard {

class ITab;

struct Bookmark {
  ITab::RuntimeID mTabID;
  PageID mPageID;
  std::string mTitle;

  constexpr bool operator==(const Bookmark&) const = default;
};

}// namespace OpenKneeboard
