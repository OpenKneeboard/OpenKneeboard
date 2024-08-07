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
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/LazyOnceValue.hpp>
#include <OpenKneeboard/StateMachine.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <wil/resource.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <format>
#include <mutex>

#include <ShlObj.h>

namespace OpenKneeboard::Filesystem {

namespace {
using LazyPath = LazyOnceValue<std::filesystem::path>;

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

std::filesystem::path GetKnownFolderPath(const _GUID& knownFolderID) {
  wil::unique_cotaskmem_ptr<wchar_t> buffer;
  winrt::check_hresult(SHGetKnownFolderPath(
    knownFolderID, KF_FLAG_CREATE, NULL, std::out_ptr(buffer)));
  return {buffer.get()};
}

static std::filesystem::path GetTemporaryDirectoryRoot() {
  static LazyPath sPath {[]() {
    wchar_t tempDirBuf[MAX_PATH];
    auto tempDirLen = GetTempPathW(MAX_PATH, tempDirBuf);
    return std::filesystem::path {std::wstring_view {tempDirBuf, tempDirLen}}
    / L"OpenKneeboard";
  }};
  return sPath;
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
  static LazyPath sPath {[]() { return GetTemporaryDirectoryImpl(); }};
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

std::filesystem::path GetCurrentExecutablePath() {
  static LazyPath sPath {[]() {
    wchar_t exePathStr[MAX_PATH];
    const auto exePathStrLen = GetModuleFileNameW(NULL, exePathStr, MAX_PATH);
    return std::filesystem::canonical(
      std::wstring_view {exePathStr, exePathStrLen});
  }};
  return sPath;
}

std::filesystem::path GetRuntimeDirectory() {
  static LazyPath sPath {
    []() { return GetCurrentExecutablePath().parent_path(); }};
  return sPath;
}

std::filesystem::path GetImmutableDataDirectory() {
  static LazyPath sPath {[]() {
    return std::filesystem::canonical(GetRuntimeDirectory() / "../share");
  }};
  return sPath;
}

std::filesystem::path GetSettingsDirectory() {
  static LazyPath sPath {[]() -> std::filesystem::path {
    const auto base = GetKnownFolderPath<FOLDERID_SavedGames>();
    if (base.empty()) {
      return {};
    }
    const auto ret = base / "OpenKneeboard";
    std::filesystem::create_directories(ret);
    return ret;
  }};

  return sPath;
}

std::filesystem::path GetLocalAppDataDirectory() {
  static LazyPath sPath {[]() -> std::filesystem::path {
    const auto base = GetKnownFolderPath<FOLDERID_LocalAppData>();
    if (base.empty()) {
      return {};
    }
    const auto ret = base / "OpenKneeboard";
    std::filesystem::create_directories(ret);
    return ret;
  }};
  return sPath;
}

std::filesystem::path GetInstalledPluginsDirectory() {
  static LazyPath sPath {[]() {
    const auto ret = GetLocalAppDataDirectory() / "Plugins" / "v1";
    std::filesystem::create_directories(ret);
    return ret;
  }};
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
  const scope_exit freePidl([pidl] { CoTaskMemFree(pidl); });
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
