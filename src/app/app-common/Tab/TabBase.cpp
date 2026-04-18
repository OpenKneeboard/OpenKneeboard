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

// Only used if the page does not have a 'real' persistent ID
static constexpr std::string_view FirstPageFallbackBookmarkPersistentId {
  "___OKB_FIRST_PAGE___"};

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

void TabBase::SetPersistentBookmarks(
  std::vector<PersistentBookmark> persistent) {
  if (this->GetPageIDs().empty()) {
    this->SetBookmarks({});
    mPendingBookmarks = std::move(persistent);
    return;
  }

  mPendingBookmarks.clear();
  // Content already loaded (e.g. folder scan completed before we were
  // called): apply immediately instead of waiting for evContentChangedEvent.
  std::vector<Bookmark> restored;
  for (auto&& [persistentID, title]: persistent) {
    if (persistentID == FirstPageFallbackBookmarkPersistentId) {
      restored.emplace_back(mRuntimeID, this->GetPageIDs().front(), title);
      continue;
    }

    const auto pageID = this->GetPageIDFromPersistentID(persistentID);
    if (pageID) {
      restored.emplace_back(mRuntimeID, *pageID, title);
    } else {
      mPendingBookmarks.emplace_back(persistentID, title);
    }
  }
  // Event listeners are not set up yet at this point, so emitting events
  // here is safe and harmless.
  this->SetBookmarks(restored);
}

std::vector<ITab::PersistentBookmark> TabBase::GetPersistentBookmarks() const {
  const auto pages = this->GetPageIDs();
  if (pages.empty()) {
    return mPendingBookmarks;
  }
  std::vector<PersistentBookmark> ret;
  ret.reserve(mBookmarks.size() + mPendingBookmarks.size());
  for (const auto& bookmark: mBookmarks) {
    const auto persistentID = this->GetPersistentIDForPage(bookmark.mPageID);
    if (persistentID) {
      ret.emplace_back(*persistentID, bookmark.mTitle);
      continue;
    }

    if (bookmark.mPageID == pages.front()) {
      ret.emplace_back(
        std::string {FirstPageFallbackBookmarkPersistentId}, bookmark.mTitle);
    }
  }
  ret.append_range(mPendingBookmarks);
  return ret;
}

void TabBase::OnContentChanged() {
  const auto pageIDs = this->GetPageIDs();
  if (pageIDs.empty()) {
    mBookmarks.clear();
    mPendingBookmarks.clear();
    return;
  }

  auto restored = mBookmarks;
  for (auto it = restored.begin(); it != restored.end();) {
    if (std::ranges::contains(pageIDs, it->mPageID)) {
      ++it;
      continue;
    }
    it = restored.erase(it);
  }
  bool modified = restored.size() != mBookmarks.size();

  for (auto it = mPendingBookmarks.begin(); it != mPendingBookmarks.end();) {
    const auto& [persistentID, title] = *it;
    const auto runtimeID = this->GetPageIDFromPersistentID(persistentID);
    if (!runtimeID) {
      ++it;
      continue;
    }

    if (std::ranges::contains(restored, *runtimeID, &Bookmark::mPageID)) {
      it = mPendingBookmarks.erase(it);
      continue;
    }

    modified = true;
    restored.emplace_back(mRuntimeID, *runtimeID, title);
    it = mPendingBookmarks.erase(it);
  }

  if (modified) {
    this->SetBookmarks(restored);
  }
}

}// namespace OpenKneeboard
