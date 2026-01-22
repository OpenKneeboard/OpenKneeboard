// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IToolbarItemWithVisibility.hpp>
#include <OpenKneeboard/ToolbarToggleAction.hpp>
#include <OpenKneeboard/UserActionHandler.hpp>

namespace OpenKneeboard {

class KneeboardState;
class KneeboardView;

class ToggleBookmarkAction final : public ToolbarToggleAction,
                                   public IToolbarItemWithVisibility,
                                   public UserActionHandler,
                                   private EventReceiver {
 public:
  ToggleBookmarkAction() = delete;
  ToggleBookmarkAction(
    KneeboardState*,
    const std::shared_ptr<KneeboardView>&,
    const std::shared_ptr<TabView>&);
  ~ToggleBookmarkAction();

  virtual bool IsVisible() const override;

  virtual bool IsActive() override;
  virtual bool IsEnabled() const override;

  [[nodiscard]]
  virtual task<void> Activate() override;
  [[nodiscard]]
  virtual task<void> Deactivate() override;
  [[nodiscard]]
  virtual task<void> Execute() override;

 private:
  KneeboardState* mKneeboardState = nullptr;
  std::weak_ptr<KneeboardView> mKneeboardView;
  std::weak_ptr<TabView> mTabView;
};

}// namespace OpenKneeboard
