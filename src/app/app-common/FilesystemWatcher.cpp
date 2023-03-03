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

#include <OpenKneeboard/FilesystemWatcher.h>

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Search;

namespace OpenKneeboard {
std::shared_ptr<FilesystemWatcher> FilesystemWatcher::Create(
  const std::filesystem::path& path) {
  auto ret = std::shared_ptr<FilesystemWatcher>(new FilesystemWatcher(path));
  ret->Initialize();
  return ret;
}

FilesystemWatcher::FilesystemWatcher(const std::filesystem::path& path)
  : mPath(path) {
}

FilesystemWatcher::~FilesystemWatcher() {
  mQueryResult = {nullptr};
}

winrt::fire_and_forget FilesystemWatcher::Initialize() {
  auto weak = weak_from_this();
  co_await winrt::resume_background();
  auto stayingAlive = weak.lock();
  if (!stayingAlive) {
    co_return;
  }

  if (!std::filesystem::exists(mPath)) {
    co_return;
  }

  const auto watchedPath
    = std::filesystem::is_directory(mPath) ? mPath : mPath.parent_path();
  mLastWriteTime = std::filesystem::last_write_time(mPath);

  auto folder
    = co_await StorageFolder::GetFolderFromPathAsync(watchedPath.wstring());
  mQueryResult = folder.CreateFileQuery();
  mQueryResult.ContentsChanged([weak](const auto&, const auto&) {
    if (auto self = weak.lock()) {
      self->OnContentsChanged();
    }
  });
  // Must fetch results once to make the query active
  co_await mQueryResult.GetFilesAsync();
}

void FilesystemWatcher::OnContentsChanged() {
  const auto stayingAlive = shared_from_this();

  if (
    (!std::filesystem::exists(mPath)) || std::filesystem::is_directory(mPath)) {
    this->evFilesystemModifiedEvent.EnqueueForContext(mOwnerThread, mPath);
    return;
  }

  // Regular file
  const auto lastWriteTime = std::filesystem::last_write_time(mPath);
  if (lastWriteTime <= mLastWriteTime) {
    return;
  }
  mLastWriteTime = lastWriteTime;
  this->evFilesystemModifiedEvent.EnqueueForContext(mOwnerThread, mPath);
}

}// namespace OpenKneeboard
