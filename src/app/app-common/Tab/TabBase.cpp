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
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/TabBase.h>

#include <unordered_set>

namespace OpenKneeboard {

static winrt::guid EnsureNonNullGuid(const winrt::guid& guid) {
  if (guid != winrt::guid {}) {
    return guid;
  }

  winrt::guid newGuid;
  winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID*>(&newGuid)));
  return newGuid;
}

TabBase::TabBase(const winrt::guid& persistentID, std::string_view title)
  : mPersistentID(EnsureNonNullGuid(persistentID)), mTitle(title) {
  AddEventListener(
    this->evContentChangedEvent,
    std::bind_front(&TabBase::OnContentChanged, this));
}

TabBase::~TabBase() {
  this->RemoveAllEventListeners();
}

ITab::RuntimeID TabBase::GetRuntimeID() const {
  return mRuntimeID;
}

winrt::guid TabBase::GetPersistentID() const {
  return mPersistentID;
}

std::string TabBase::GetTitle() const {
  return mTitle;
}

void TabBase::SetTitle(const std::string& title) {
  mTitle = title;
  evSettingsChangedEvent.Emit();
}

std::vector<Bookmark> TabBase::GetBookmarks() const {
  return mBookmarks;
}

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
}

void TabBase::OnContentChanged() {
  decltype(mBookmarks) bookmarks;
  const auto pageIDs = this->GetPageIDs();
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
