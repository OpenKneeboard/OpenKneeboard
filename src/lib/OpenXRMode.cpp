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
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <fmt/format.h>
#include <fmt/xchar.h>

// clang-format off
#include <Windows.h>
#include <Psapi.h>
// clang-format on

namespace OpenKneeboard {

void SetOpenXRModeWithHelperProcess(OpenXRMode mode) {
  if (mode == OpenXRMode::Disabled) {
    return;// TODO
  }

  auto layerPath = RuntimeFiles::GetDirectory().wstring();
  auto exePath = (RuntimeFiles::GetDirectory()
                  / RuntimeFiles::OPENXR_REGISTER_LAYER_HELPER)
                   .wstring();

  auto commandLine = fmt::format(L"\"{}\" \"{}\"", exePath, layerPath);
  commandLine.reserve(32767);

  STARTUPINFOEXW startupInfo {};
  startupInfo.StartupInfo.cb = sizeof(startupInfo);
  SIZE_T attributeListSize;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);
  startupInfo.lpAttributeList
    = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attributeListSize));
  DWORD policy = PROCESS_CREATION_DESKTOP_APP_BREAKAWAY_ENABLE_PROCESS_TREE;
  UpdateProcThreadAttribute(
    startupInfo.lpAttributeList,
    0,
    PROC_THREAD_ATTRIBUTE_DESKTOP_APP_POLICY,
    &policy,
    sizeof(policy),
    nullptr,
    nullptr);

  PROCESS_INFORMATION procInfo {};

  CreateProcessW(
    exePath.c_str(),
    commandLine.data(),
    nullptr,
    nullptr,
    FALSE,
    0,
    nullptr,
    nullptr,
    reinterpret_cast<LPSTARTUPINFOW>(&startupInfo),
    &procInfo);
  free(startupInfo.lpAttributeList);
}

}// namespace OpenKneeboard
