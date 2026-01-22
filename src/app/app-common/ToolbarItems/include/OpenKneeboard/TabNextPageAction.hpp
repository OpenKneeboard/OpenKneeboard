// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ToolbarAction.hpp>
#include <OpenKneeboard/UserActionHandler.hpp>

namespace OpenKneeboard {

class TabView;

class TabNextPageAction final : public ToolbarAction,
                                private EventReceiver,
                                public UserActionHandler {
 public:
  TabNextPageAction(KneeboardState*, const std::shared_ptr<TabView>& state);
  TabNextPageAction() = delete;

  ~TabNextPageAction();

  virtual bool IsEnabled() const override;
  [[nodiscard]]
  virtual task<void> Execute() override;

 private:
  KneeboardState* mKneeboard = nullptr;
  std::weak_ptr<TabView> mTabView;
};

}// namespace OpenKneeboard
