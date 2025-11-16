// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/FilesystemWatcher.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Storage.Search.h>

#include <OpenKneeboard/audited_ptr.hpp>

#include <filesystem>
#include <memory>

namespace OpenKneeboard {

class PlainTextPageSource;

class PlainTextFilePageSource final : public PageSourceWithDelegates {
 private:
  PlainTextFilePageSource(const audited_ptr<DXResources>&, KneeboardState*);

 public:
  PlainTextFilePageSource() = delete;
  virtual ~PlainTextFilePageSource();
  static task<std::shared_ptr<PlainTextFilePageSource>> Create(
    audited_ptr<DXResources>,
    KneeboardState*,
    std::filesystem::path path = {});

  std::filesystem::path GetPath() const;
  void SetPath(const std::filesystem::path& path);

  PageIndex GetPageCount() const override;

  void Reload();

 private:
  winrt::apartment_context mUIThread;
  std::filesystem::path mPath;
  std::shared_ptr<PlainTextPageSource> mPageSource;

  std::string GetFileContent() const;

  std::shared_ptr<FilesystemWatcher> mWatcher;

  void SubscribeToChanges() noexcept;
  void OnFileModified(const std::filesystem::path&) noexcept;
};

}// namespace OpenKneeboard
