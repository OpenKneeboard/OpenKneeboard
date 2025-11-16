// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
#include "IPageSource.hpp"
namespace OpenKneeboard {

class IPageSourceWithDeveloperTools : public virtual IPageSource {
 public:
  [[nodiscard]]
  virtual bool HasDeveloperTools(PageID) const
    = 0;
  virtual fire_and_forget OpenDeveloperToolsWindow(KneeboardViewID, PageID) = 0;
};
}// namespace OpenKneeboard