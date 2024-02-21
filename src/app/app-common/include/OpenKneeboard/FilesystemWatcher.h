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

#include <OpenKneeboard/Events.h>

#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/handles.h>

#include <shims/filesystem>
#include <shims/winrt/base.h>

#include <winrt/Windows.Storage.Search.h>

#include <memory>

#include <fileapi.h>

namespace OpenKneeboard {

class FilesystemWatcher final
  : public std::enable_shared_from_this<FilesystemWatcher> {
 public:
  using unique_changenotification = std::
    unique_ptr<HANDLE, CHandleDeleter<HANDLE, &FindCloseChangeNotification>>;
  static std::shared_ptr<FilesystemWatcher> Create(
    const std::filesystem::path&);

  static winrt::fire_and_forget final_release(
    std::unique_ptr<FilesystemWatcher>);

  Event<std::filesystem::path> evFilesystemModifiedEvent;

  FilesystemWatcher() = delete;
  ~FilesystemWatcher();

 private:
  FilesystemWatcher(const std::filesystem::path&);
  void Initialize();
  winrt::Windows::Foundation::IAsyncAction Run();
  winrt::fire_and_forget OnContentsChanged();

  winrt::Windows::Foundation::IAsyncAction mImpl {nullptr};

  winrt::apartment_context mOwnerThread;
  std::filesystem::path mPath;
  std::filesystem::file_time_type mLastWriteTime;
  bool mSettling = false;

  unique_changenotification mHandle;
  std::stop_source mStop;
  winrt::handle mShutdownHandle;
};

}// namespace OpenKneeboard
