// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ISelectableToolbarItem.hpp>

#include <vector>

namespace OpenKneeboard {

class IToolbarFlyout : public virtual ISelectableToolbarItem {
 public:
  virtual ~IToolbarFlyout() = default;

  virtual std::vector<std::shared_ptr<IToolbarItem>> GetSubItems() const = 0;
};

}// namespace OpenKneeboard
