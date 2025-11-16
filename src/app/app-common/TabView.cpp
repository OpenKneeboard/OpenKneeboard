// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/IPageSourceWithCursorEvents.hpp>
#include <OpenKneeboard/IPageSourceWithNavigation.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/NavigationTab.hpp>
#include <OpenKneeboard/TabView.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

namespace OpenKneeboard {

TabView::TabView(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const std::shared_ptr<ITab>& tab,
  KneeboardViewID id)
  : mDXR(dxr), mKneeboard(kneeboard), mRootTab(tab), mKneeboardViewID(id) {
  const auto rootPageIDs = tab->GetPageIDs();
  if (!rootPageIDs.empty()) {
    mRootTabPage = {rootPageIDs.front(), 0};
  }

  AddEventListener(tab->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  AddEventListener(
    tab->evContentChangedEvent,
    std::bind_front(&TabView::OnTabContentChanged, this));
  AddEventListener(
    tab->evPageAppendedEvent,
    std::bind_front(&TabView::OnTabPageAppended, this));
  AddEventListener(
    this->evPageChangedEvent, this->evAvailableFeaturesChangedEvent);
  AddEventListener(
    tab->evPageChangeRequestedEvent, [this](KneeboardViewID ctx, PageID id) {
      if (ctx != mKneeboardViewID) {
        return;
      }
      this->SetPageID(id);
    });
  AddEventListener(
    tab->evAvailableFeaturesChangedEvent,
    this->evAvailableFeaturesChangedEvent);
  AddEventListener(tab->evBookmarksChangedEvent, this->evBookmarksChangedEvent);
}

TabView::~TabView() {
  this->RemoveAllEventListeners();
}

std::weak_ptr<ITab> TabView::GetRootTab() const {
  return mRootTab;
}

std::weak_ptr<ITab> TabView::GetTab() const {
  return mActiveSubTab ? mActiveSubTab : mRootTab;
}

PageID TabView::GetPageID() const {
  const auto mode = this->GetTabMode();
  if (mode != TabMode::Normal && mActiveSubTabPageID) {
    return *mActiveSubTabPageID;
  }
  if (mode == TabMode::Normal && mRootTabPage) {
    return mRootTabPage->mID;
  }

  auto tab = this->GetTab().lock();
  if (!tab) {
    return PageID {nullptr};
  }
  const auto ids = tab->GetPageIDs();
  if (ids.size() == 0) {
    return PageID {nullptr};
  }

  return ids.front();
}

std::vector<PageID> TabView::GetPageIDs() const {
  if (mActiveSubTab) {
    return mActiveSubTab->GetPageIDs();
  }
  auto tab = mRootTab.lock();
  if (!tab) {
    return {};
  }
  return tab->GetPageIDs();
}

void TabView::PostCursorEvent(const CursorEvent& ev) {
  auto receiver = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(
    this->GetTab().lock());
  if (!receiver) {
    return;
  }
  const auto size = this->GetPreferredSize();
  if (!size.has_value()) {
    return;
  }
  CursorEvent tabEvent {ev};
  tabEvent.mX *= size->mPixelSize.mWidth;
  tabEvent.mY *= size->mPixelSize.mHeight;
  receiver->PostCursorEvent(mKneeboardViewID, tabEvent, this->GetPageID());
}

void TabView::SetPageID(PageID page) {
  const auto tab = this->GetTab().lock();
  if (!tab) {
    return;
  }
  const auto pages = tab->GetPageIDs();
  const auto it = std::ranges::find(pages, page);
  if (it == pages.end()) {
    return;
  }

  if (mActiveSubTab) {
    mActiveSubTabPageID = page;
  } else {
    mRootTabPage = {page, static_cast<PageIndex>(it - pages.begin())};
  }

  this->PostCursorEvent({});

  evNeedsRepaintEvent.Emit();
  evPageChangedEvent.Emit();
}

void TabView::OnTabContentChanged() {
  const scope_exit updateOnExit([this] {
    this->evContentChangedEvent.Emit();
    if (!mActiveSubTab) {
      evNeedsRepaintEvent.Emit();
    }
  });

  const auto tab = mRootTab.lock();
  if (!tab) {
    return;
  }

  const auto pages = tab->GetPageIDs();
  if (pages.empty()) {
    mRootTabPage = {};
    evPageChangedEvent.Emit();
    return;
  }

  if (mRootTabPage) {
    const auto it = std::ranges::find(pages, mRootTabPage->mID);
    if (it == pages.end()) {
      // Fixed below
      mRootTabPage = {};
    }
  }

  if (!mRootTabPage) {
    mRootTabPage = {pages.front(), 0};
    evPageChangedEvent.Emit();
    return;
  }

  const auto it = std::ranges::find(pages, mRootTabPage->mID);
  if (it == pages.end()) {
    mRootTabPage = {pages.back(), static_cast<PageIndex>(pages.size() - 1)};
    evPageChangedEvent.Emit();
    return;
  }

  if (mRootTabPage->mIndex == 0 && it != pages.begin()) {
    mRootTabPage->mID = pages.front();
    evPageChangedEvent.Emit();
    return;
  }
}

void TabView::OnTabPageAppended(SuggestedPageAppendAction suggestedAction) {
  auto tab = mRootTab.lock();
  if (!tab) {
    return;
  }

  auto pages = tab->GetPageIDs();
  if (pages.size() < 2 || !mRootTabPage) {
    mRootTabPage = {pages.front(), 0};
    evPageChangedEvent.Emit();
    evNeedsRepaintEvent.Emit();
    return;
  }

  if (suggestedAction == SuggestedPageAppendAction::KeepOnCurrentPage) {
    return;
  }

  if (mRootTabPage->mIndex != pages.size() - 2) {
    return;
  }

  mRootTabPage = {pages.back(), static_cast<PageIndex>(pages.size() - 1)};
  if (mActiveSubTab) {
    return;
  }

  evPageChangedEvent.Emit();
  evNeedsRepaintEvent.Emit();
}

std::optional<PreferredSize> TabView::GetPreferredSize() const {
  auto tab = this->GetTab().lock();
  if (!tab) {
    return std::nullopt;
  }
  const auto currentPage = this->GetPageID();
  if (!std::ranges::contains(tab->GetPageIDs(), currentPage)) {
    return std::nullopt;
  }
  return tab->GetPreferredSize(currentPage);
}

TabMode TabView::GetTabMode() const {
  return mTabMode;
}

bool TabView::SupportsTabMode(TabMode mode) const {
  switch (mode) {
    case TabMode::Normal:
      return true;
    case TabMode::Navigation: {
      auto tab = mRootTab.lock();
      if (!tab) {
        return false;
      }
      auto nav = std::dynamic_pointer_cast<IPageSourceWithNavigation>(tab);
      return nav && nav->IsNavigationAvailable();
    }
  }
  // above switch should be exhaustive
  OPENKNEEBOARD_BREAK;
  return false;
}

bool TabView::SetTabMode(TabMode mode) {
  if (mTabMode == mode || !SupportsTabMode(mode)) {
    return false;
  }

  auto receiver = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(
    this->GetTab().lock());
  if (receiver) {
    receiver->PostCursorEvent(mKneeboardViewID, {}, this->GetPageID());
  }

  mTabMode = mode;
  mActiveSubTab = nullptr;
  mActiveSubTabPageID = {};

  switch (mode) {
    case TabMode::Normal:
      break;
    case TabMode::Navigation: {
      OPENKNEEBOARD_TraceLoggingScope(
        "TabView::SetTabMode(TabMode::Navigation)");
      auto tab = mRootTab.lock();
      if (!tab) {
        return false;
      }
      mActiveSubTab = std::make_shared<NavigationTab>(
        mDXR,
        tab,
        std::dynamic_pointer_cast<IPageSourceWithNavigation>(tab)
          ->GetNavigationEntries());
      AddEventListener(
        mActiveSubTab->evPageChangeRequestedEvent,
        [this](KneeboardViewID ctx, PageID newPage) {
          if (ctx != mKneeboardViewID) {
            return;
          }
          auto tab = mRootTab.lock();
          if (!tab) {
            return;
          }
          const auto ids = tab->GetPageIDs();
          const auto it = std::ranges::find(ids, newPage);
          if (it == ids.end()) {
            return;
          }
          mRootTabPage = {newPage, static_cast<PageIndex>(it - ids.begin())};
          SetTabMode(TabMode::Normal);
        });
      AddEventListener(
        mActiveSubTab->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
      break;
    }
  }

  if (mode != TabMode::Normal && !mActiveSubTab) {
    // Switch should have been exhaustive
    OPENKNEEBOARD_BREAK;
  }

  evPageChangedEvent.Emit();
  evNeedsRepaintEvent.Emit();
  evTabModeChangedEvent.Emit();
  evContentChangedEvent.Emit();

  return true;
}

}// namespace OpenKneeboard
