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
#include <shims/winrt.h>

namespace OpenKneeboard::RuntimeFiles {

std::filesystem::path GetDirectory() {
  static std::filesystem::path sPath;
  if (!sPath.empty()) {
    return sPath;
  }

  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(NULL, buf, MAX_PATH);
  sPath = std::filesystem::canonical(std::filesystem::path(buf).parent_path());
  return sPath;
}

}// namespace OpenKneeboard::RuntimeFiles
