// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/TabNavigationAction.hpp>
#include <OpenKneeboard/TabView.hpp>

namespace OpenKneeboard {

TabNavigationAction::TabNavigationAction(const std::shared_ptr<TabView>& state)
  : ToolbarToggleAction("\uE700", _("Contents")),
    mTabView(state) {
  AddEventListener(
    state->evAvailableFeaturesChangedEvent, this->evStateChangedEvent);
  AddEventListener(state->evContentChangedEvent, this->evStateChangedEvent);
}

TabNavigationAction::~TabNavigationAction() { this->RemoveAllEventListeners(); }

bool TabNavigationAction::IsEnabled() const {
  auto tv = mTabView.lock();
  if (!tv) {
    return false;
  }
  return tv->SupportsTabMode(TabMode::Navigation);
}

bool TabNavigationAction::IsActive() {
  auto tv = mTabView.lock();
  return tv && tv->GetTabMode() == TabMode::Navigation;
}

task<void> TabNavigationAction::Activate() {
  if (auto tv = mTabView.lock()) {
    tv->SetTabMode(TabMode::Navigation);
  }
  co_return;
}

task<void> TabNavigationAction::Deactivate() {
  if (auto tv = mTabView.lock()) {
    tv->SetTabMode(TabMode::Normal);
  }
  co_return;
}

}// namespace OpenKneeboard
