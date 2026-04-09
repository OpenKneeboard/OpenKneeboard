// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/FilesystemWatcher.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <shims/winrt/base.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

namespace OpenKneeboard {

class KneeboardState;

class FolderPageSource final : public PageSourceWithDelegates {
 private:
  FolderPageSource(const audited_ptr<DXResources>&, KneeboardState*);

 public:
  FolderPageSource() = delete;
  static task<std::shared_ptr<FolderPageSource>>
  Create(audited_ptr<DXResources>, KneeboardState*, std::filesystem::path = {});
  virtual ~FolderPageSource();

  std::filesystem::path GetPath() const;
  [[nodiscard]]
  task<void> SetPath(std::filesystem::path path);

  [[nodiscard]]
  task<void> Reload() noexcept;

  // Returns the path relative to GetPath() and the local page index within
  // that file for a given global PageID; returns nullopt if not found.
  [[nodiscard]]
  std::optional<std::pair<std::filesystem::path, PageIndex>>
  GetFileAndLocalIndexForPageID(PageID id) const;

  // Returns the global PageID for a file given by a path relative to
  // GetPath() and a local page index within that file; returns nullopt if
  // the file is not loaded or localIndex is out of range.
  [[nodiscard]]
  std::optional<PageID> GetPageIDForRelativeFile(
    const std::filesystem::path& relativeFile,
    PageIndex localIndex) const;

  virtual std::optional<nlohmann::json> GetPersistentIDForPage(
    PageID) const override;
  virtual std::optional<PageID> GetPageIDFromPersistentID(
    const nlohmann::json&) const override;

 private:
  void SubscribeToChanges();
  OpenKneeboard::fire_and_forget OnFileModified(std::filesystem::path);

  winrt::apartment_context mUIThread;
  std::shared_ptr<FilesystemWatcher> mWatcher;

  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard = nullptr;

  std::filesystem::path mPath;
  struct DelegateInfo {
    std::filesystem::file_time_type mModified;
    std::shared_ptr<IPageSource> mDelegate;
  };
  std::map<std::filesystem::path, DelegateInfo> mContents;
};

}// namespace OpenKneeboard
