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

/** A separate process to register the OpenXR layer, outside of the
 * MSIX sandbox.
 *
 * If done from the main process, the registry write will be app-specific.
 */

#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>
#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>

using namespace OpenKneeboard;

static void InstallOpenXRLayer(const std::filesystem::path& directory) {
  HKEY openXRKey {0};
  RegCreateKeyExW(
    HKEY_CURRENT_USER,
    L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit",
    0,
    nullptr,
    0,
    KEY_ALL_ACCESS,
    nullptr,
    &openXRKey,
    nullptr);
  if (!openXRKey) {
    dprint("Failed to open or create HKCU OpenXR key");
    return;
  }
  auto jsonPath
    = std::filesystem::canonical(directory / RuntimeFiles::OPENXR_JSON)
        .wstring();
  DWORD disabled = 0;
  if (
    RegQueryValueExW(
      openXRKey, jsonPath.c_str(), NULL, nullptr, nullptr, nullptr)
    == ERROR_FILE_NOT_FOUND) {
    RegSetValueExW(
      openXRKey,
      jsonPath.c_str(),
      0,
      REG_DWORD,
      reinterpret_cast<const BYTE*>(&disabled),
      sizeof(disabled));
  }

  // https://docs.microsoft.com/en-us/windows/win32/sysinfo/registry-element-size-limits
  std::wstring valueNameBuffer(16383, L'x');
  DWORD valueSize = valueNameBuffer.size();
  DWORD valueIndex {0};
  disabled = 1;
  while (RegEnumValueW(
           openXRKey,
           valueIndex++,
           valueNameBuffer.data(),
           &valueSize,
           nullptr,
           nullptr,
           nullptr,
           nullptr)
         == ERROR_SUCCESS) {
    std::wstring valueName = valueNameBuffer.substr(0, valueSize);
    valueSize = valueNameBuffer.size();

    if (
      valueName.ends_with(RuntimeFiles::OPENXR_JSON.filename().wstring())
      && valueName != jsonPath) {
      RegSetValueExW(
        openXRKey,
        valueName.c_str(),
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&disabled),
        sizeof(disabled));
    }
  }
  RegCloseKey(openXRKey);
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
  DPrintSettings::Set({
    .prefix = "OpenKneeboard-OpenXR-RegisterLayer",
  });
  int argc = 0;
  auto argv = CommandLineToArgvW(commandLine, &argc);
  if (argc != 1) {
    dprintf("Invalid arguments ({}):", argc);
    for (int i = 0; i < argc; ++i) {
      dprintf(L"argv[{}]: {}", i, argv[i]);
    }
  }
  dprintf(L"Registering OpenXR layer at {}", argv[0]);
  InstallOpenXRLayer(std::wstring_view(argv[0]));
  return 0;
}
