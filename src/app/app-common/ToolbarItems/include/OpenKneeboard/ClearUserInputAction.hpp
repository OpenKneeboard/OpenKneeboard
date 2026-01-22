// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IToolbarItemWithConfirmation.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>

#include <OpenKneeboard/inttypes.hpp>

namespace OpenKneeboard {

class TabView;
class KneeboardState;
class KneeboardView;

class ClearUserInputAction final : public ToolbarAction,
                                   public EventReceiver,
                                   public virtual IToolbarItemWithConfirmation {
 public:
  ClearUserInputAction(
    KneeboardState*,
    const std::shared_ptr<TabView>&,
    CurrentPage_t);
  ClearUserInputAction(
    KneeboardState*,
    const std::shared_ptr<TabView>&,
    AllPages_t);
  ClearUserInputAction(KneeboardState*, AllTabs_t);
  ClearUserInputAction() = delete;

  ~ClearUserInputAction();

  virtual bool IsEnabled() const override;
  [[nodiscard]]
  virtual task<void> Execute() override;

  virtual std::string_view GetConfirmationTitle() const override;
  virtual std::string_view GetConfirmationDescription() const override;
  virtual std::string_view GetConfirmButtonLabel() const override;
  virtual std::string_view GetCancelButtonLabel() const override;

 private:
  void SubscribeToEvents();

  enum class Mode { CurrentPage, ThisTab, AllTabs };
  Mode mMode;
  KneeboardState* mKneeboardState = nullptr;
  std::weak_ptr<TabView> mTabView;
};

}// namespace OpenKneeboard
