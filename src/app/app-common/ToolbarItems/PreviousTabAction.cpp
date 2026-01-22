// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/AppSettings.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/PreviousTabAction.hpp>
#include <OpenKneeboard/TabsList.hpp>

namespace OpenKneeboard {

PreviousTabAction::PreviousTabAction(
  KneeboardState* kneeboardState,
  const std::shared_ptr<KneeboardView>& kneeboardView)
  : ToolbarAction("\uE74A", _("Previous Tab")),
    mKneeboardState(kneeboardState),
    mKneeboardView(kneeboardView) {
  AddEventListener(
    kneeboardView->evCurrentTabChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    kneeboardState->evSettingsChangedEvent, this->evStateChangedEvent);
}

PreviousTabAction::~PreviousTabAction() { this->RemoveAllEventListeners(); }

bool PreviousTabAction::IsEnabled() const {
  auto kbv = mKneeboardView.lock();
  if (!kbv) {
    return false;
  }

  if (kbv->GetTabIndex() > 0) {
    return true;
  }

  if (mKneeboardState->GetTabsList()->GetTabs().size() < 2) {
    return false;
  }

  return mKneeboardState->GetUISettings().mLoopTabs;
}

task<void> PreviousTabAction::Execute() {
  if (auto kbv = mKneeboardView.lock()) {
    kbv->PreviousTab();
  }
  co_return;
}

}// namespace OpenKneeboard
