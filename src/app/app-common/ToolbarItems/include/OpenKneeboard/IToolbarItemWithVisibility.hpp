// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IToolbarItem.hpp>

namespace OpenKneeboard {

class IToolbarItemWithVisibility : public virtual IToolbarItem {
 public:
  virtual ~IToolbarItemWithVisibility() = default;

  virtual bool IsVisible() const = 0;
};

}// namespace OpenKneeboard
