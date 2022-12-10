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

#include <functional>
#include <shims/filesystem>
#include <string>

using namespace OpenKneeboard;

static HKEY OpenOrCreateImplicitLayerRegistryKey(HKEY root) {
  HKEY openXRKey {0};
  const auto result = RegCreateKeyExW(
    root,
    L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit",
    0,
    nullptr,
    0,
    KEY_ALL_ACCESS,
    nullptr,
    &openXRKey,
    nullptr);
  if (result != ERROR_SUCCESS) {
    dprintf("Failed to open OpenXR implicit layer key: {}", result);
  }
  return openXRKey;
}

static void DisableOpenXRLayers(
  HKEY root,
  std::function<bool(std::wstring_view)> predicate) {
  auto openXRKey = OpenOrCreateImplicitLayerRegistryKey(root);
  if (!openXRKey) {
    return;
  }

  // https://docs.microsoft.com/en-us/windows/win32/sysinfo/registry-element-size-limits
  std::wstring valueNameBuffer(16383, L'x');
  DWORD valueSize = valueNameBuffer.size();
  DWORD valueIndex {0};
  DWORD disabled = 1;
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

    if (predicate(valueName)) {
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

static void DisableOpenXRLayer(
  HKEY root,
  const std::filesystem::path& directory) {
  auto jsonPath
    = std::filesystem::canonical(directory / RuntimeFiles::OPENXR_JSON)
        .wstring();

  DisableOpenXRLayers(root, [jsonPath](std::wstring_view layerPath) {
    return layerPath == jsonPath;
  });
}

static void EnableOpenXRLayer(
  HKEY root,
  const std::filesystem::path& directory) {
  auto openXRKey = OpenOrCreateImplicitLayerRegistryKey(root);
  if (!openXRKey) {
    dprint("Failed to open or create OpenXR key");
    return;
  }

  const auto jsonFile = RuntimeFiles::OPENXR_JSON.filename().wstring();
  auto jsonPath
    = std::filesystem::canonical(directory / RuntimeFiles::OPENXR_JSON)
        .wstring();
  DisableOpenXRLayers(root, [jsonFile, jsonPath](std::wstring_view layerPath) {
    return layerPath != jsonPath && layerPath.ends_with(jsonFile);
  });

  DWORD disabled = 0;
  const auto success = RegSetValueExW(
    openXRKey,
    jsonPath.c_str(),
    0,
    REG_DWORD,
    reinterpret_cast<const BYTE*>(&disabled),
    sizeof(disabled));
  if (success != ERROR_SUCCESS) {
    dprintf("Failed to set OpenXR key: {}", success);
  }

  RegCloseKey(openXRKey);
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
  DPrintSettings::Set({
    .prefix = "OpenXR-Helper",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });
  int argc = 0;
  auto argv = CommandLineToArgvW(commandLine, &argc);
  if (argc != 2) {
    dprintf("Invalid arguments ({}):", argc);
    for (int i = 0; i < argc; ++i) {
      dprintf(L"argv[{}]: {}", i, argv[i]);
    }
    return 1;
  }
  dprintf(L"OpenXR: {} -> {}", argv[0], argv[1]);
  const auto command = std::wstring_view(argv[0]);
  const auto path = std::wstring_view(argv[1]);
  if (command == L"disable-HKCU") {
    DisableOpenXRLayer(HKEY_CURRENT_USER, path);
    return 0;
  }
  if (command == L"disable-HKLM") {
    DisableOpenXRLayer(HKEY_LOCAL_MACHINE, path);
    return 0;
  }
  if (command == L"enable-HKCU") {
    EnableOpenXRLayer(HKEY_CURRENT_USER, path);
    return 0;
  }
  if (command == L"enable-HKLM") {
    EnableOpenXRLayer(HKEY_LOCAL_MACHINE, path);
    return 0;
  }
  dprintf(L"Invalid command: {}", command);
  return 1;
}
