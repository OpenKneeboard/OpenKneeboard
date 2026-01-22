// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ISelectableToolbarItem.hpp>

namespace OpenKneeboard {

class IToolbarItemWithConfirmation : public virtual ISelectableToolbarItem {
 public:
  virtual ~IToolbarItemWithConfirmation() = default;

  virtual std::string_view GetConfirmationTitle() const = 0;
  virtual std::string_view GetConfirmationDescription() const = 0;
  virtual std::string_view GetConfirmButtonLabel() const = 0;
  virtual std::string_view GetCancelButtonLabel() const = 0;
};

}// namespace OpenKneeboard
