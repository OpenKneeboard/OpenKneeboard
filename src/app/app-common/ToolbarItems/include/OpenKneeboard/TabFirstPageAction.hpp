// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ToolbarAction.hpp>

namespace OpenKneeboard {

class TabView;

class TabFirstPageAction final : public ToolbarAction, private EventReceiver {
 public:
  TabFirstPageAction() = delete;
  TabFirstPageAction(const std::shared_ptr<TabView>& state);
  ~TabFirstPageAction();

  virtual bool IsEnabled() const override;
  [[nodiscard]]
  virtual task<void> Execute() override;

 private:
  std::weak_ptr<TabView> mTabView;
};

}// namespace OpenKneeboard
