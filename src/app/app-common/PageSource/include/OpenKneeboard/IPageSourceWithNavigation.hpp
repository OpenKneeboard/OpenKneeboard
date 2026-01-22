// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IPageSource.hpp>
#include <OpenKneeboard/UniqueID.hpp>

#include <OpenKneeboard/utf8.hpp>

namespace OpenKneeboard {

struct NavigationEntry {
  std::string mName;
  PageID mPageID;
};

class IPageSourceWithNavigation : public virtual IPageSource {
 public:
  virtual bool IsNavigationAvailable() const = 0;
  virtual std::vector<NavigationEntry> GetNavigationEntries() const = 0;
};

}// namespace OpenKneeboard
