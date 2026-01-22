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

class KneeboardState;

class SwitchProfileAction final : public ToolbarAction,
                                  public EventReceiver,
                                  public ICheckableToolbarItem {
 public:
  SwitchProfileAction(
    KneeboardState*,
    const winrt::guid& profileID,
    const std::string& profileName);
  ~SwitchProfileAction();

  virtual bool IsEnabled() const override;
  virtual bool IsChecked() const override;
  [[nodiscard]]
  virtual task<void> Execute() override;

  SwitchProfileAction() = delete;

 private:
  KneeboardState* mKneeboardState;
  winrt::guid mProfileID;
};

}// namespace OpenKneeboard
