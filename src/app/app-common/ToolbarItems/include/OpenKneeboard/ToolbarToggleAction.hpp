// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ToolbarAction.hpp>

namespace OpenKneeboard {

class ToolbarToggleAction : public ToolbarAction {
 public:
  [[nodiscard]]
  virtual task<void> Execute() override;

  virtual bool IsActive() = 0;
  [[nodiscard]]
  virtual task<void> Activate()
    = 0;
  [[nodiscard]]
  virtual task<void> Deactivate()
    = 0;

 protected:
  using ToolbarAction::ToolbarAction;
};

}// namespace OpenKneeboard
