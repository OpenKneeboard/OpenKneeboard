// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/TabFirstPageAction.hpp>
#include <OpenKneeboard/TabView.hpp>

namespace OpenKneeboard {

TabFirstPageAction::TabFirstPageAction(const std::shared_ptr<TabView>& state)
  : ToolbarAction("\uE892", _("First Page")),
    mTabView(state) {
  AddEventListener(state->evPageChangedEvent, this->evStateChangedEvent);
  AddEventListener(state->evContentChangedEvent, this->evStateChangedEvent);
}

TabFirstPageAction::~TabFirstPageAction() { this->RemoveAllEventListeners(); }

bool TabFirstPageAction::IsEnabled() const {
  auto tv = mTabView.lock();
  if (!tv) {
    return false;
  }
  const auto pageIDs = tv->GetPageIDs();
  return !(pageIDs.empty() || tv->GetPageID() == pageIDs.front());
}

task<void> TabFirstPageAction::Execute() {
  auto tv = mTabView.lock();
  if (!tv) {
    co_return;
  }

  const auto pageIDs = tv->GetPageIDs();
  if (!pageIDs.empty()) {
    tv->SetPageID(pageIDs.front());
  }
}

}// namespace OpenKneeboard
