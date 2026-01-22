// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ToolbarToggleAction.hpp>

namespace OpenKneeboard {

class TabView;

class TabNavigationAction final : public ToolbarToggleAction,
                                  private EventReceiver {
 public:
  TabNavigationAction() = delete;
  TabNavigationAction(const std::shared_ptr<TabView>& state);
  ~TabNavigationAction();

  virtual bool IsActive() override;
  virtual bool IsEnabled() const override;

  [[nodiscard]]
  virtual task<void> Activate() override;
  [[nodiscard]]
  virtual task<void> Deactivate() override;

 private:
  std::weak_ptr<TabView> mTabView;
};

}// namespace OpenKneeboard
