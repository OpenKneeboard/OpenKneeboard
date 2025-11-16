// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IToolbarItemWithConfirmation.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>
#include <OpenKneeboard/UserActionHandler.hpp>

namespace OpenKneeboard {

class TabView;
class KneeboardState;
class KneeboardView;

class ReloadTabAction final : public ToolbarAction,
                              public EventReceiver,
                              public UserActionHandler,
                              public virtual IToolbarItemWithConfirmation {
 public:
  ReloadTabAction(KneeboardState*, const std::shared_ptr<TabView>&);
  ReloadTabAction(KneeboardState*, AllTabs_t);

  ~ReloadTabAction();

  virtual bool IsEnabled() const override;
  virtual task<void> Execute() override;

  virtual std::string_view GetConfirmationTitle() const override;
  virtual std::string_view GetConfirmationDescription() const override;
  virtual std::string_view GetConfirmButtonLabel() const override;
  virtual std::string_view GetCancelButtonLabel() const override;

  ReloadTabAction() = delete;

 private:
  enum class Mode {
    AllTabs,
    ThisTab,
  };
  Mode mMode;
  KneeboardState* mKneeboardState;

  std::weak_ptr<TabView> mTabView;
};

}// namespace OpenKneeboard
