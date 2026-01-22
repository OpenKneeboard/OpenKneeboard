// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/AppSettings.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/TabPreviousPageAction.hpp>
#include <OpenKneeboard/TabView.hpp>

namespace OpenKneeboard {

TabPreviousPageAction::TabPreviousPageAction(
  KneeboardState* kneeboard,
  const std::shared_ptr<TabView>& tabView)
  : ToolbarAction("\uE760", _("Previous Page")),
    mKneeboard(kneeboard),
    mTabView(tabView) {
  AddEventListener(tabView->evPageChangedEvent, this->evStateChangedEvent);
  AddEventListener(tabView->evContentChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    kneeboard->evSettingsChangedEvent, this->evStateChangedEvent);
  if (auto tab = tabView->GetTab().lock()) {
    AddEventListener(tab->evPageAppendedEvent, this->evStateChangedEvent);
  }
}

TabPreviousPageAction::~TabPreviousPageAction() {
  this->RemoveAllEventListeners();
}

bool TabPreviousPageAction::IsEnabled() const {
  auto tv = mTabView.lock();
  if (!tv) {
    return false;
  }

  const auto pages = tv->GetPageIDs();
  if (pages.size() < 2) {
    return false;
  }

  if (mKneeboard->GetUISettings().mLoopPages) {
    return true;
  }

  return tv->GetPageID() != pages.front();
}

task<void> TabPreviousPageAction::Execute() {
  auto tv = mTabView.lock();
  if (!tv) {
    co_return;
  }

  const auto pages = tv->GetPageIDs();

  if (pages.size() < 2) {
    co_return;
  }

  auto it = std::ranges::find(pages, tv->GetPageID());
  if (it == pages.end()) {
    co_return;
  }

  if (it != pages.begin()) {
    it--;
  } else if (mKneeboard->GetUISettings().mLoopPages) {
    it = pages.end() - 1;
  } else {
    co_return;
  }

  tv->SetPageID(*it);
}

}// namespace OpenKneeboard
