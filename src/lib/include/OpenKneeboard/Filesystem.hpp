// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <filesystem>
#include <thread>

struct _GUID;

namespace OpenKneeboard::Filesystem {

// Using shortcuts rather than symlinks because symlinks require admin

bool IsDirectoryShortcut(const std::filesystem::path&) noexcept;
// Argument order matches std::filesystem::create_directory_symlink()
void CreateDirectoryShortcut(
  const std::filesystem::path& target,
  const std::filesystem::path& link) noexcept;

/** Differs from std::filesystem::temp_directory_path() in that
 * it guarantees to be in canonical form */
std::filesystem::path GetTemporaryDirectory();
std::filesystem::path GetLocalAppDataDirectory();
std::filesystem::path GetLogsDirectory();
std::filesystem::path GetCrashLogsDirectory();
std::filesystem::path GetRuntimeDirectory();
std::filesystem::path GetImmutableDataDirectory();
std::filesystem::path GetSettingsDirectory();
std::filesystem::path GetInstalledPluginsDirectory();
std::filesystem::path GetCurrentExecutablePath();

std::filesystem::path GetKnownFolderPath(const _GUID& knownFolderID);

template <const _GUID& knownFolderID>
auto GetKnownFolderPath() {
  static std::once_flag sOnce;
  static std::filesystem::path sPath;
  std::call_once(
    sOnce, [&path = sPath]() { path = GetKnownFolderPath(knownFolderID); });
  return sPath;
}

void OpenExplorerWithSelectedFile(const std::filesystem::path& path);

/// Migrate from `Saved Games\OpenKneeboard` to
/// `%LOCALAPPDATA%\OpenKneeboard\Settings`
void MigrateSettingsDirectory();

void CleanupTemporaryDirectories();

class ScopedDeleter {
 public:
  ScopedDeleter(const std::filesystem::path&);
  ~ScopedDeleter() noexcept;

  ScopedDeleter() = delete;
  ScopedDeleter(const ScopedDeleter&) = delete;
  ScopedDeleter& operator=(const ScopedDeleter&) = delete;

 private:
  std::filesystem::path mPath;
};

class TemporaryCopy final {
 public:
  TemporaryCopy(
    const std::filesystem::path& source,
    const std::filesystem::path& destination);
  ~TemporaryCopy() noexcept;

  TemporaryCopy() = delete;
  TemporaryCopy(const TemporaryCopy&) = delete;
  TemporaryCopy& operator=(const TemporaryCopy&) = delete;

  std::filesystem::path GetPath() const noexcept;

 private:
  std::filesystem::path mCopy;
};

}// namespace OpenKneeboard::Filesystem
