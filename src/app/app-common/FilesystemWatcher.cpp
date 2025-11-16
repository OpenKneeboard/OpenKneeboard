// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/FilesystemWatcher.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/task/resume_on_signal.hpp>

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
    /* watch subtree = */
    true,
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
      | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
  if (handle == INVALID_HANDLE_VALUE) {
    dprint("Invalid handle: {}", GetLastError());
    OPENKNEEBOARD_BREAK;
    return;
  }
  mHandle.reset(handle);
}

OpenKneeboard::fire_and_forget FilesystemWatcher::final_release(
  std::unique_ptr<FilesystemWatcher> self) {
  self->mStop.request_stop();
  co_await std::move(self->mImpl).value();
}

FilesystemWatcher::~FilesystemWatcher() = default;

void FilesystemWatcher::Initialize() {
  mLastWriteTime = std::filesystem::last_write_time(mPath);
  mImpl = this->Run();
}

task<void> FilesystemWatcher::Run() {
  auto handle = mHandle.get();
  auto stop = mStop.get_token();
  while (!stop.stop_requested()) {
    winrt::check_bool(FindNextChangeNotification(handle));
    co_await resume_on_signal(handle, stop);
    if (stop.stop_requested()) {
      co_return;
    }
    if (!mSettling) {
      this->OnContentsChanged();
    }
  }
}

OpenKneeboard::fire_and_forget FilesystemWatcher::OnContentsChanged() {
  const auto weak = weak_from_this();

  try {
    if (
      (!std::filesystem::exists(mPath))
      || std::filesystem::is_directory(mPath)) {
      this->evFilesystemModifiedEvent.EnqueueForContext(mOwnerThread, mPath);
      co_return;
    }
  } catch (const std::filesystem::filesystem_error& e) {
    // Do nothing: assume an operation is in progress, so we'll get a further
    // notification when it's done - or just torn down and a new watcher created
    dprint("Error checking if path exists or is directory: {}", e.what());
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
  const scope_exit settled([this]() { mSettling = false; });

  while (true) {
    auto self = weak.lock();
    if (!self) {
      co_return;
    }

    std::filesystem::file_time_type lastWriteTime {};
    try {
      lastWriteTime = std::filesystem::last_write_time(mPath);
    } catch (const std::filesystem::filesystem_error& e) {
      // Probably deleted
      dprint(
        "Getting last write time for path '{}' failed: {:#08x} - {}",
        mPath,
        static_cast<uint32_t>(e.code().value()),
        e.what());
      this->evFilesystemModifiedEvent.EnqueueForContext(mOwnerThread, mPath);
      co_return;
    }
    if (lastWriteTime == mLastWriteTime) {
      co_return;
    }

    {
      const auto handle = Win32::CreateFile(
        mPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
      if (!handle) {
        const auto error = GetLastError();
        if (error == ERROR_SHARING_VIOLATION) {
          TraceLoggingWrite(
            gTraceProvider,
            "FilesystemWatcher::OnContentsChanged/ExclusiveToOtherProcess");
          co_await winrt::resume_after(std::chrono::milliseconds(100));
          continue;
        } else {
          dprint(
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