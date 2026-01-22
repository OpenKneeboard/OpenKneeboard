// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/TabNextPageAction.hpp>
#include <OpenKneeboard/TabView.hpp>

namespace OpenKneeboard {

TabNextPageAction::TabNextPageAction(
  KneeboardState* kneeboard,
  const std::shared_ptr<TabView>& tabView)
  : ToolbarAction("\uE761", _("Next Page")),
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

TabNextPageAction::~TabNextPageAction() { this->RemoveAllEventListeners(); }

bool TabNextPageAction::IsEnabled() const {
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

  return tv->GetPageID() != pages.back();
}

task<void> TabNextPageAction::Execute() {
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

  it++;
  if (it == pages.end()) {
    if (mKneeboard->GetUISettings().mLoopPages) {
      it = pages.begin();
    } else {
      co_return;
    }
  }

  tv->SetPageID(*it);
}

}// namespace OpenKneeboard
