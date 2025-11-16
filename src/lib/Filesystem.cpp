// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/LazyOnceValue.hpp>
#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>
#include <ShlObj.h>

#include <wil/resource.h>

#include <format>
#include <fstream>
#include <mutex>

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

bool IsDirectoryShortcut(const std::filesystem::path& link) noexcept {
  if (!std::filesystem::exists(link)) {
    return false;
  }

  const auto shortcut
    = winrt::create_instance<IShellLinkW>(CLSID_FolderShortcut);
  const auto persist = shortcut.as<IPersistFile>();
  return SUCCEEDED(persist->Load(link.wstring().c_str(), STGM_READ));
}

// Argument order matches std::filesystem::create_directory_symlink()
void CreateDirectoryShortcut(
  const std::filesystem::path& target,
  const std::filesystem::path& link) noexcept {
  auto shortcut = winrt::create_instance<IShellLinkW>(CLSID_FolderShortcut);
  shortcut->SetPath(target.wstring().c_str());
  shortcut->SetDescription(std::format(L"Shortcut to {}", target).c_str());

  auto persist = shortcut.as<IPersistFile>();
  winrt::check_hresult(persist->Save(link.wstring().c_str(), TRUE));
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
  dprint("Cleaning temporary directory root: {}", root);
  if (!std::filesystem::exists(root)) {
    return;
  }

  std::error_code ignored;
  for (const auto& it: std::filesystem::directory_iterator(root)) {
    std::filesystem::remove_all(it.path(), ignored);
  }

  dprint("New temporary directory: {}", Filesystem::GetTemporaryDirectory());
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
    const auto base = GetKnownFolderPath<FOLDERID_LocalAppData>();
    if (base.empty()) {
      return {};
    }
    const auto ret = base / "OpenKneeboard" / "Settings";
    std::filesystem::create_directories(ret);
    return ret;
  }};

  return sPath;
}

void MigrateSettingsDirectory() {
  const auto newPath = GetSettingsDirectory();
  if (!std::filesystem::is_empty(newPath)) {
    return;
  }
  const auto oldPath
    = GetKnownFolderPath<FOLDERID_SavedGames>() / "OpenKneeboard";
  if (!std::filesystem::exists(oldPath)) {
    return;
  }

  dprint("🚚 moving settings from `{}` to `{}`", oldPath, newPath);

  bool canDelete = true;

  std::filesystem::remove(newPath);
  for (auto&& it: std::filesystem::recursive_directory_iterator(oldPath)) {
    if (it.is_directory()) {
      continue;
    }

    const auto src = it.path();
    if (!src.has_extension()) {
      continue;
    }

    if (src.filename() == ".instance") {
      continue;
    }

    const auto ext = src.extension();

    if (ext == ".dmp" || ext == ".log") {
      continue;
    }

    if (ext != ".json") {
      // Some people put content in their OpenKneeboard folder :/
      canDelete = false;
      continue;
    }

    auto dest = newPath;
    for (auto&& part: std::filesystem::relative(src.parent_path(), oldPath)
           / src.filename()) {
      if (part == "profiles") {
        dest /= "Profiles";
      } else {
        dest /= part;
      }
    }

    dprint("🚚 `{}` -> `{}`", src, dest);
    std::filesystem::create_directories(dest.parent_path());
    std::filesystem::rename(src, dest);
  }

  if (canDelete) {
    std::filesystem::remove_all(oldPath);
  }

  const auto warningFile = canDelete
    ? (oldPath.parent_path() / "OpenKneeboard-README.txt")
    : (oldPath / "SETTINGS_HAVE_MOVED-README.txt");
  {
    std::ofstream f(warningFile, std::ios::binary | std::ios::trunc);
    f << "OpenKneeboard's settings have been moved to:\n"
      << newPath.string() << std::endl;

    if (!canDelete) {
      f << "\nThis folder has been left here in case you want to keep any "
           "other files you may have put in it."
        << std::endl;
    }
  }
  dprint("✅ moved, and created warning file at `{}`", warningFile);
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

std::filesystem::path GetLogsDirectory() {
  static LazyPath sPath {[]() -> std::filesystem::path {
    const auto oldPath = GetLocalAppDataDirectory() / "Logs";
    const auto path
      = GetKnownFolderPath<FOLDERID_LocalAppData>() / "OpenKneeboard Logs";

    if (std::filesystem::exists(oldPath) && !std::filesystem::exists(path)) {
      std::filesystem::rename(oldPath, path);
    }

    std::filesystem::create_directories(path);

    if (!Filesystem::IsDirectoryShortcut(oldPath)) {
      if (std::filesystem::exists(oldPath)) {
        std::filesystem::remove_all(oldPath);
      }
      Filesystem::CreateDirectoryShortcut(path, oldPath);
    }
    return path;
  }};
  return sPath;
}

std::filesystem::path GetCrashLogsDirectory() {
  static LazyPath sPath {[]() -> std::filesystem::path {
    const auto ret = GetLogsDirectory() / "Crashes";
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
    dprint("{} - path '{}' does not exist (yet?)", __FUNCTION__, path);
    OPENKNEEBOARD_BREAK;
    return;
  }
  if (!std::filesystem::is_regular_file(path)) {
    dprint("{} - path '{}' is not a file", __FUNCTION__, path);
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