// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ICheckableToolbarItem.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>

namespace OpenKneeboard {

class ITab;
class KneeboardView;

class SwitchTabAction final : public ToolbarAction,
                              public EventReceiver,
                              public ICheckableToolbarItem {
 public:
  SwitchTabAction(
    const std::shared_ptr<KneeboardView>&,
    const std::shared_ptr<ITab>&);
  ~SwitchTabAction();

  virtual bool IsEnabled() const override;
  virtual bool IsChecked() const override;
  [[nodiscard]]
  virtual task<void> Execute() override;

  SwitchTabAction() = delete;

 private:
  std::weak_ptr<KneeboardView> mKneeboardView;
  ITab::RuntimeID mTabID;
};

}// namespace OpenKneeboard
