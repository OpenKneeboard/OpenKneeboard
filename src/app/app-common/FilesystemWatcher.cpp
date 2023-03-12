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
  auto ret = shared_with_final_release(new FilesystemWatcher(path));
  ret->Initialize();
  return ret;
}

FilesystemWatcher::FilesystemWatcher(const std::filesystem::path& path)
  : mPath(path) {
  const auto watchedPath
    = std::filesystem::is_directory(mPath) ? mPath : mPath.parent_path();
  const auto handle = FindFirstChangeNotificationW(
    watchedPath.wstring().c_str(),
    /* watch subtree = */ true,
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
      | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
  if (handle == INVALID_HANDLE_VALUE) {
    dprintf("Invalid handle: {}", GetLastError());
    OPENKNEEBOARD_BREAK;
    return;
  }
  mHandle.reset(handle);
  mShutdownHandle = {CreateEventW(nullptr, FALSE, FALSE, nullptr)};
}

winrt::fire_and_forget FilesystemWatcher::final_release(
  std::unique_ptr<FilesystemWatcher> self) {
  self->mCanceled = true;
  co_await winrt::resume_on_signal(self->mShutdownHandle.get());
}

FilesystemWatcher::~FilesystemWatcher() = default;

winrt::fire_and_forget FilesystemWatcher::Initialize() {
  mLastWriteTime = std::filesystem::last_write_time(mPath);

  auto handle = mHandle.get();
  while (!mCanceled) {
    winrt::check_bool(FindNextChangeNotification(handle));
    const auto haveChange = co_await winrt::resume_on_signal(
      handle, std::chrono::milliseconds(100));
    if (haveChange) {
      this->OnContentsChanged();
    } else {
      continue;
    }
  }
  SetEvent(mShutdownHandle.get());
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
