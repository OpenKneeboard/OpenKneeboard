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
#include <OpenKneeboard/RuntimeFiles.h>

// clang-format off
#include <Windows.h>
#include <ShlObj.h>
#include <Psapi.h>
#include <appmodel.h>
// clang-format on

#include <OpenKneeboard/dprint.h>
#include <shims/winrt.h>

namespace OpenKneeboard::RuntimeFiles {

std::filesystem::path GetDirectory() {
  static std::filesystem::path sPath;
  if (!sPath.empty()) {
    return sPath;
  }

  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(NULL, buf, MAX_PATH);
  const auto executablePath
    = std::filesystem::canonical(std::filesystem::path(buf).parent_path());

  UINT32 packageNameLength {0};
  if (
    GetCurrentPackageFullName(&packageNameLength, nullptr)
    == APPMODEL_ERROR_NO_PACKAGE) {
    // Not running from an installed package, can use inject DLLs directly from
    // our executable directory
    sPath = executablePath;
    return sPath;
  }

  // App data directory is not readable by other apps if using msix installer,
  // so if we pass a DLL in the app directory to `LoadLibraryW` in another
  // process, it will fail. Copy them out to a readable directory.
  wchar_t* ref = nullptr;
  winrt::check_hresult(
    SHGetKnownFolderPath(FOLDERID_LocalAppData, NULL, NULL, &ref));
  sPath = std::filesystem::path(std::wstring_view(ref)) / "OpenKneeboard" / "Shared";
  std::filesystem::create_directories(sPath);

#define IT(file) \
  try { \
    std::filesystem::copy( \
      executablePath / file, \
      sPath / file, \
      std::filesystem::copy_options::overwrite_existing); \
  } catch (std::filesystem::filesystem_error & e) { \
    dprintf("Injectable DLL copy failed: {}", e.what()); \
  }

  OPENKNEEBOARD_RUNTIME_FILES
#undef IT
  return sPath;
}

}// namespace OpenKneeboard::RuntimeFiles
