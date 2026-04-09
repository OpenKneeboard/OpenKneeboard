// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/TabBase.hpp>

#include <unordered_set>

namespace OpenKneeboard {

static winrt::guid EnsureNonNullGuid(const winrt::guid& guid) {
  if (guid != winrt::guid {}) {
    return guid;
  }

  return random_guid();
}

TabBase::TabBase(const winrt::guid& persistentID, std::string_view title)
  : mPersistentID(EnsureNonNullGuid(persistentID)),
    mTitle(title) {
  AddEventListener(
    this->evContentChangedEvent,
    std::bind_front(&TabBase::OnContentChanged, this));
}

TabBase::~TabBase() { this->RemoveAllEventListeners(); }

ITab::RuntimeID TabBase::GetRuntimeID() const { return mRuntimeID; }

winrt::guid TabBase::GetPersistentID() const { return mPersistentID; }

std::string TabBase::GetTitle() const { return mTitle; }

void TabBase::SetTitle(const std::string& title) {
  if (title == mTitle) {
    return;
  }
  mTitle = title;
  evSettingsChangedEvent.Emit();
}

std::vector<Bookmark> TabBase::GetBookmarks() const { return mBookmarks; }

void TabBase::SetBookmarks(const std::vector<Bookmark>& bookmarks) {
  std::unordered_set<PageID> seenPages;
  for (const auto& bookmark: bookmarks) {
    if (bookmark.mTabID != mRuntimeID) {
      throw std::logic_error("Trying to set bookmark for a different tab");
    }

    if (seenPages.contains(bookmark.mPageID)) {
      throw std::logic_error("Trying to add two bookmarks for the same page");
    }
    seenPages.insert(bookmark.mPageID);
  }

  mBookmarks = bookmarks;
  evAvailableFeaturesChangedEvent.Emit();
  evBookmarksChangedEvent.Emit();
  evSettingsChangedEvent.Emit();
}

void TabBase::SetPendingBookmarkRestore(std::vector<PendingBookmark> pending) {
  if (!this->GetPageIDs().empty()) {
    // Content already loaded (e.g. folder scan completed before we were called):
    // apply immediately instead of waiting for evContentChangedEvent.
    std::vector<Bookmark> restored;
    for (const auto& bm: pending) {
      if (auto pageID = this->GetPageIDFromPersistentID(bm.mPersistentID)) {
        restored.push_back({mRuntimeID, *pageID, bm.mTitle});
      }
    }
    // Event listeners are not set up yet at this point, so emitting events
    // here is safe and harmless.
    this->SetBookmarks(restored);
    return;
  }
  mPendingBookmarks = std::move(pending);
}

std::vector<TabBase::PendingBookmark> TabBase::GetPendingBookmarkData() const {
  return mPendingBookmarks.value_or(std::vector<PendingBookmark> {});
}

void TabBase::OnContentChanged() {
  const auto pageIDs = this->GetPageIDs();

  if (mPendingBookmarks.has_value()) {
    if (!pageIDs.empty()) {
      std::vector<Bookmark> restored;
      for (const auto& bm: *mPendingBookmarks) {
        if (auto pageID = this->GetPageIDFromPersistentID(bm.mPersistentID)) {
          restored.push_back({mRuntimeID, *pageID, bm.mTitle});
        }
      }
      mPendingBookmarks.reset();
      this->SetBookmarks(restored);
    }
    return;
  }

  decltype(mBookmarks) bookmarks;
  for (const auto& bookmark: mBookmarks) {
    if (std::ranges::find(pageIDs, bookmark.mPageID) != pageIDs.end()) {
      bookmarks.push_back(bookmark);
    }
  }
  if (bookmarks.size() != mBookmarks.size()) {
    this->SetBookmarks(bookmarks);
  }
}

}// namespace OpenKneeboard
