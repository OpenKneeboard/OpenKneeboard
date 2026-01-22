// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IToolbarFlyout.hpp>

#include <vector>

namespace OpenKneeboard {

class ToolbarFlyout : public IToolbarFlyout, public EventReceiver {
 public:
  ToolbarFlyout(
    std::string glyph,
    std::string label,
    const std::vector<std::shared_ptr<IToolbarItem>>& items);
  ToolbarFlyout() = delete;
  virtual ~ToolbarFlyout();

  virtual bool IsEnabled() const override;

  std::string_view GetGlyph() const override final;
  std::string_view GetLabel() const override final;
  virtual std::vector<std::shared_ptr<IToolbarItem>> GetSubItems()
    const override;

 private:
  std::string mGlyph;
  std::string mLabel;
  std::vector<std::shared_ptr<IToolbarItem>> mSubItems;
};

}// namespace OpenKneeboard
