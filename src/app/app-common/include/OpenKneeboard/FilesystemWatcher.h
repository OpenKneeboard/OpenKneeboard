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

#include <shims/filesystem>
#include <shims/winrt/base.h>

#include <winrt/Windows.Storage.Search.h>

#include <memory>

namespace OpenKneeboard {

class FilesystemWatcher final
  : public std::enable_shared_from_this<FilesystemWatcher> {
 public:
  static std::shared_ptr<FilesystemWatcher> Create(
    const std::filesystem::path&);

  Event<std::filesystem::path> evFilesystemModifiedEvent;

  FilesystemWatcher() = delete;
  ~FilesystemWatcher();

 private:
  FilesystemWatcher(const std::filesystem::path&);
  winrt::fire_and_forget Initialize();
  void OnContentsChanged();

  winrt::apartment_context mOwnerThread;
  std::filesystem::path mPath;
  std::filesystem::file_time_type mLastWriteTime;
  winrt::Windows::Storage::Search::StorageFileQueryResult mQueryResult {
    nullptr};
};

}// namespace OpenKneeboard
