// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "ITab.hpp"

#include <OpenKneeboard/Bookmark.hpp>

#include <cstdint>
#include <optional>

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

  // Pending bookmark data for file-based tabs (SingleFileTab).
  // mFileHash is carried so that a startup save does not lose the hash
  // before content finishes loading.
  struct PendingBookmarkData {
    uint64_t mFileHash {0};
    std::vector<std::pair<PageIndex, std::string>> mPages;
  };

  void SetPendingBookmarkRestore(PendingBookmarkData pending);

  // Returns pending bookmark data for pass-through serialization during
  // startup, before content is loaded and pending bookmarks have been applied.
  std::optional<PendingBookmarkData> GetPendingBookmarkData() const;

 protected:
  TabBase(const winrt::guid& persistentID, std::string_view title);

 private:
  const winrt::guid mPersistentID;
  const RuntimeID mRuntimeID;
  std::string mTitle;
  std::vector<Bookmark> mBookmarks;
  std::optional<PendingBookmarkData> mPendingBookmarks;

  void OnContentChanged();
};

}// namespace OpenKneeboard
