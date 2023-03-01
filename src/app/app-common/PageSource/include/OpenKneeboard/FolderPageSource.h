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
#pragma once

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>
#include <shims/winrt/base.h>
#include <winrt/Windows.Storage.Search.h>

#include <memory>
#include <shims/filesystem>

namespace OpenKneeboard {

struct KneeboardState;

class FolderPageSource final
  : public PageSourceWithDelegates,
    public std::enable_shared_from_this<FolderPageSource> {
 private:
  FolderPageSource(const DXResources&, KneeboardState*);

 public:
  FolderPageSource() = delete;
  static std::shared_ptr<FolderPageSource> Create(
    const DXResources&,
    KneeboardState*,
    const std::filesystem::path& = {});
  virtual ~FolderPageSource();

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

  winrt::fire_and_forget Reload() noexcept;

 private:
  winrt::apartment_context mUIThread;

  DXResources mDXR;
  KneeboardState* mKneeboard = nullptr;

  std::filesystem::path mPath;
  struct DelegateInfo {
    std::filesystem::file_time_type mModified;
    std::shared_ptr<IPageSource> mDelegate;
  };
  std::map<std::filesystem::path, DelegateInfo> mContents;

  winrt::Windows::Storage::Search::StorageFileQueryResult mQueryResult {
    nullptr};
};

}// namespace OpenKneeboard
