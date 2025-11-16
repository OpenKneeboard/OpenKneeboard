// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/ToolbarToggleAction.hpp>

namespace OpenKneeboard {

task<void> ToolbarToggleAction::Execute() {
  if (!this->IsEnabled()) {
    co_return;
  }

  if (this->IsActive()) {
    co_await this->Deactivate();
    co_return;
  }
  co_await this->Activate();
}

}// namespace OpenKneeboard
