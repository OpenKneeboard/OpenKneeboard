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

#include <OpenKneeboard/scope_guard.h>

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
  self->mImpl.Cancel();
  co_await winrt::resume_on_signal(self->mShutdownHandle.get());
}

FilesystemWatcher::~FilesystemWatcher() = default;

void FilesystemWatcher::Initialize() {
  mLastWriteTime = std::filesystem::last_write_time(mPath);
  mImpl = this->Run();
}

winrt::Windows::Foundation::IAsyncAction FilesystemWatcher::Run() {
  auto cancellation_token = co_await winrt::get_cancellation_token();
  cancellation_token.enable_propagation();

  auto handle = mHandle.get();
  while (!cancellation_token()) {
    winrt::check_bool(FindNextChangeNotification(handle));
    try {
      const auto haveChange = co_await winrt::resume_on_signal(handle);
      if (haveChange && !mSettling) {
        this->OnContentsChanged();
      }
    } catch (const winrt::hresult_canceled&) {
      SetEvent(mShutdownHandle.get());
      co_return;
    }
  }
}

winrt::fire_and_forget FilesystemWatcher::OnContentsChanged() {
  const auto weak = weak_from_this();

  if (
    (!std::filesystem::exists(mPath)) || std::filesystem::is_directory(mPath)) {
    this->evFilesystemModifiedEvent.EnqueueForContext(mOwnerThread, mPath);
    co_return;
  }

  // Regular file

  constexpr const auto tickDelta = std::chrono::file_clock::duration(1);
  // This specific value isn't required by OpenKneeboard; this check is just
  // paranoia making sure that `std::chrono::file_clock` matches the
  // documented tick length for the Windows FILETIME structure as of
  // 2023-03-26.
  static_assert(tickDelta == std::chrono::nanoseconds(100));

  mSettling = true;
  const scope_guard settled([this]() { mSettling = false; });

  while (true) {
    auto self = weak.lock();
    if (!self) {
      co_return;
    }

    const auto lastWriteTime = std::filesystem::last_write_time(mPath);
    if (lastWriteTime <= mLastWriteTime) {
      co_return;
    }

    {
      winrt::file_handle handle {CreateFileW(
        mPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL)};
      if (!handle) {
        const auto error = GetLastError();
        if (error == ERROR_SHARING_VIOLATION) {
          traceprint("File opened exclusively by another process, waiting");
          co_await winrt::resume_after(std::chrono::milliseconds(100));
          continue;
        } else {
          dprintf(
            L"Failed to open modified file '{}': {}", mPath.wstring(), error);
          co_return;
        }
      }
    }

    const auto now = std::chrono::file_clock::now();
    if (now - lastWriteTime < tickDelta) {
      co_await winrt::resume_after(tickDelta);
      continue;
    }

    mLastWriteTime = lastWriteTime;
    this->evFilesystemModifiedEvent.EnqueueForContext(mOwnerThread, mPath);
    co_return;
  }
}

}// namespace OpenKneeboard
