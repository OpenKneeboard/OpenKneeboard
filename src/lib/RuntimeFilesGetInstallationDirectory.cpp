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
// clang-format on

#include <OpenKneeboard/dprint.h>
#include <shims/winrt/base.h>

namespace OpenKneeboard::RuntimeFiles {

std::filesystem::path GetInstallationDirectory() {
  static std::filesystem::path sPath;
  if (!sPath.empty()) {
    return sPath;
  }

  // App bin directory is not readable by other apps if using msix installer,
  // so if we pass a DLL in the app directory to `LoadLibraryW` in another
  // process, it will fail. Copy them out to a readable directory.
  wchar_t* ref = nullptr;
  winrt::check_hresult(
    SHGetKnownFolderPath(FOLDERID_ProgramData, NULL, NULL, &ref));
  sPath = std::filesystem::path(std::wstring_view(ref)) / "OpenKneeboard";

  return sPath;
}

}// namespace OpenKneeboard::RuntimeFiles
