// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/FilesystemWatcher.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>

#include <filesystem>
#include <memory>

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
