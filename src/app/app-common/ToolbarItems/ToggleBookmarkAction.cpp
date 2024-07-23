/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
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
  return mKneeboardState->GetAppSettings().mBookmarks.mEnabled;
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

winrt::Windows::Foundation::IAsyncAction ToggleBookmarkAction::Activate() {
  if (IsActive()) {
    co_return;
  }

  const auto view = mKneeboardView.lock();
  if (!view) {
    co_return;
  }
  view->AddBookmarkForCurrentPage();
}

winrt::Windows::Foundation::IAsyncAction ToggleBookmarkAction::Deactivate() {
  if (!IsActive()) {
    co_return;
  }

  const auto view = mKneeboardView.lock();
  if (!view) {
    co_return;
  }
  view->RemoveBookmarkForCurrentPage();
}

winrt::Windows::Foundation::IAsyncAction ToggleBookmarkAction::Execute() {
  // Re-implemented to put in UserActionHandler vtable
  co_await ToolbarToggleAction::Execute();
}

}// namespace OpenKneeboard
