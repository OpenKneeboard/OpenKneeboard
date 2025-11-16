// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/ToggleBookmarkAction.hpp>

namespace OpenKneeboard {

ToggleBookmarkAction::ToggleBookmarkAction(
  KneeboardState* kneeboardState,
  const std::shared_ptr<KneeboardView>& kneeboardView,
  const std::shared_ptr<TabView>& tabView)
  : ToolbarToggleAction("\uE840", _("Pin")),
    mKneeboardState(kneeboardState),
    mKneeboardView(kneeboardView),
    mTabView(tabView) {
  AddEventListener(
    kneeboardState->evSettingsChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    tabView->evAvailableFeaturesChangedEvent, this->evStateChangedEvent);
}

ToggleBookmarkAction::~ToggleBookmarkAction() {
  this->RemoveAllEventListeners();
}

bool ToggleBookmarkAction::IsVisible() const {
  return mKneeboardState->GetUISettings().mBookmarks.mEnabled;
}

bool ToggleBookmarkAction::IsEnabled() const {
  auto tabView = mTabView.lock();
  if (!tabView) {
    return false;
  }

  if (tabView->GetTabMode() != TabMode::Normal) {
    return false;
  }

  if (tabView->GetPageIDs().size() == 0) {
    return false;
  }

  return true;
}

bool ToggleBookmarkAction::IsActive() {
  const auto view = mKneeboardView.lock();
  if (!view) {
    return false;
  }
  return view->CurrentPageHasBookmark();
}

task<void> ToggleBookmarkAction::Activate() {
  if (IsActive()) {
    co_return;
  }

  const auto view = mKneeboardView.lock();
  if (!view) {
    co_return;
  }
  view->AddBookmarkForCurrentPage();
}

task<void> ToggleBookmarkAction::Deactivate() {
  if (!IsActive()) {
    co_return;
  }

  const auto view = mKneeboardView.lock();
  if (!view) {
    co_return;
  }
  view->RemoveBookmarkForCurrentPage();
}

task<void> ToggleBookmarkAction::Execute() {
  // Re-implemented to put in UserActionHandler vtable
  co_await ToolbarToggleAction::Execute();
}

}// namespace OpenKneeboard
