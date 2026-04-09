// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "ITab.hpp"

#include <OpenKneeboard/Bookmark.hpp>

#include <shims/nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace OpenKneeboard {

class TabBase : public virtual ITab, public virtual EventReceiver {
 public:
  TabBase() = delete;
  ~TabBase();

  virtual RuntimeID GetRuntimeID() const override final;
  virtual winrt::guid GetPersistentID() const override final;
  virtual std::string GetTitle() const override final;
  virtual void SetTitle(const std::string&) override final;

  virtual std::vector<Bookmark> GetBookmarks() const override final;
  virtual void SetBookmarks(const std::vector<Bookmark>&) override final;

  // A bookmark to be restored once the page source has finished loading.
  // The persistent ID is opaque data returned by IPageSource::GetPersistentIDForPage().
  struct PendingBookmark {
    nlohmann::json mPersistentID;
    std::string mTitle;
  };

  void SetPendingBookmarkRestore(std::vector<PendingBookmark> pending);

  // Returns pending bookmarks for pass-through serialization during startup,
  // before content is loaded and pending bookmarks have been applied.
  std::vector<PendingBookmark> GetPendingBookmarkData() const;

 protected:
  TabBase(const winrt::guid& persistentID, std::string_view title);

 private:
  const winrt::guid mPersistentID;
  const RuntimeID mRuntimeID;
  std::string mTitle;
  std::vector<Bookmark> mBookmarks;
  std::optional<std::vector<PendingBookmark>> mPendingBookmarks;

  void OnContentChanged();
};

}// namespace OpenKneeboard
