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
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/StateMachine.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/hresult.h>
#include <OpenKneeboard/scope_guard.h>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <wil/resource.h>

#include <ztd/out_ptr.hpp>

#include <format>
#include <mutex>

#include <ShlObj.h>

namespace OpenKneeboard::Filesystem {

namespace {

enum class TemporaryDirectoryState {
  Uninitialized,
  Cleaned,
  Initialized,
};

AtomicStateMachine<
  TemporaryDirectoryState,
  TemporaryDirectoryState::Uninitialized,
  std::array {
    Transition {
      TemporaryDirectoryState::Uninitialized,
      TemporaryDirectoryState::Cleaned,
    },
    Transition {
      TemporaryDirectoryState::Cleaned,
      TemporaryDirectoryState::Initialized,
    },
  }>
  gTemporaryDirectoryState {};
}// namespace

std::filesystem::path GetKnownFolderPath(const GUID& knownFolderID) {
  wil::unique_cotaskmem_ptr<wchar_t> buffer;
  winrt::check_hresult(SHGetKnownFolderPath(
    knownFolderID, NULL, NULL, ztd::out_ptr::out_ptr(buffer)));
  return {buffer.get()};
}

static std::filesystem::path GetTemporaryDirectoryRoot() {
  wchar_t tempDirBuf[MAX_PATH];
  auto tempDirLen = GetTempPathW(MAX_PATH, tempDirBuf);
  return std::filesystem::path {std::wstring_view {tempDirBuf, tempDirLen}}
  / L"OpenKneeboard";
}

static std::filesystem::path GetTemporaryDirectoryImpl() {
  auto tempDir = GetTemporaryDirectoryRoot()
    / std::format("{:%F %H-%M-%S} {}",

                  std::chrono::floor<std::chrono::seconds>(
                    std::chrono::system_clock::now()),
                  GetCurrentProcessId());

  if (!std::filesystem::exists(tempDir)) {
    std::filesystem::create_directories(tempDir);
  }

  gTemporaryDirectoryState.Transition<
    TemporaryDirectoryState::Cleaned,
    TemporaryDirectoryState::Initialized>();

  return std::filesystem::canonical(tempDir);
}

std::filesystem::path GetTemporaryDirectory() {
  static std::filesystem::path sPath;
  static std::once_flag sFlag;

  std::call_once(
    sFlag, [&path = sPath]() { path = GetTemporaryDirectoryImpl(); });
  return sPath;
}

void CleanupTemporaryDirectories() {
  gTemporaryDirectoryState.Transition<
    TemporaryDirectoryState::Uninitialized,
    TemporaryDirectoryState::Cleaned>();
  const auto root = GetTemporaryDirectoryRoot();
  dprintf("Cleaning temporary directory root: {}", root);
  if (!std::filesystem::exists(root)) {
    return;
  }

  std::error_code ignored;
  for (const auto& it: std::filesystem::directory_iterator(root)) {
    std::filesystem::remove_all(it.path(), ignored);
  }

  dprintf("New temporary directory: {}", Filesystem::GetTemporaryDirectory());
}

std::filesystem::path GetRuntimeDirectory() {
  static std::filesystem::path sCache;
  if (!sCache.empty()) {
    return sCache;
  }

  wchar_t exePathStr[MAX_PATH];
  const auto exePathStrLen = GetModuleFileNameW(NULL, exePathStr, MAX_PATH);

  const std::filesystem::path exePath
    = std::filesystem::canonical(std::wstring_view {exePathStr, exePathStrLen});

  sCache = exePath.parent_path();
  return sCache;
}

std::filesystem::path GetImmutableDataDirectory() {
  static std::filesystem::path sCache;
  if (sCache.empty()) {
    sCache = std::filesystem::canonical(GetRuntimeDirectory() / "../share");
  }
  return sCache;
}

std::filesystem::path GetSettingsDirectory() {
  static std::filesystem::path sPath;
  static std::once_flag sFlag;

  std::call_once(sFlag, [p = &sPath]() {
    const auto base = GetKnownFolderPath<FOLDERID_SavedGames>();
    if (base.empty()) {
      return;
    }
    *p = base / "OpenKneeboard";
    std::filesystem::create_directories(*p);
  });

  return sPath;
}

std::filesystem::path GetLocalAppDataDirectory() {
  static std::filesystem::path sPath;
  static std::once_flag sFlag;

  std::call_once(sFlag, [p = &sPath]() {
    const auto base = GetKnownFolderPath<FOLDERID_LocalAppData>();
    if (base.empty()) {
      return;
    }
    *p = base / "OpenKneeboard";
    std::filesystem::create_directories(*p);
  });

  return sPath;
}

void OpenExplorerWithSelectedFile(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    dprintf("{} - path '{}' does not exist (yet?)", __FUNCTION__, path);
    OPENKNEEBOARD_BREAK;
    return;
  }
  if (!std::filesystem::is_regular_file(path)) {
    dprintf("{} - path '{}' is not a file", __FUNCTION__, path);
    OPENKNEEBOARD_BREAK;
    return;
  }
  PIDLIST_ABSOLUTE pidl {nullptr};
  check_hresult(
    SHParseDisplayName(path.wstring().c_str(), nullptr, &pidl, 0, nullptr));
  const scope_guard freePidl([pidl] { CoTaskMemFree(pidl); });
  check_hresult(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0));
}

ScopedDeleter::ScopedDeleter(const std::filesystem::path& path) : mPath(path) {
}

ScopedDeleter::~ScopedDeleter() noexcept {
  if (std::filesystem::exists(mPath)) {
    std::filesystem::remove_all(mPath);
  }
}

TemporaryCopy::TemporaryCopy(
  const std::filesystem::path& source,
  const std::filesystem::path& destination) {
  if (!std::filesystem::exists(source)) {
    throw std::logic_error("TemporaryCopy created without a source file");
  }
  if (std::filesystem::exists(destination)) {
    throw std::logic_error(
      "TemporaryCopy created, but destination already exists");
  }
  std::filesystem::copy(source, destination);
  mCopy = destination;
}

TemporaryCopy::~TemporaryCopy() noexcept {
  std::filesystem::remove(mCopy);
}

std::filesystem::path TemporaryCopy::GetPath() const noexcept {
  return mCopy;
}
};// namespace OpenKneeboard::Filesystem
