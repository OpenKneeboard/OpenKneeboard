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
#include <OpenKneeboard/utf8.h>
#include <Windows.h>
#include <shellapi.h>
#include <shims/winrt/base.h>

#include <format>
#include <shims/filesystem>
#include <string>

using namespace OpenKneeboard;

static HKEY OpenImplicitLayerRegistryKey(HKEY root) {
  HKEY openXRKey {0};
  RegOpenKeyExW(
    root,
    L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit",
    0,
    KEY_ALL_ACCESS,
    &openXRKey);
  return openXRKey;
}

static void DisableOpenKneeboardOpenXRLayers(HKEY root) {
  auto openXRKey = OpenImplicitLayerRegistryKey(root);
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

    if (valueName.ends_with(L"OpenKneeboard-OpenXR.json")) {
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

void RemoveRuntimeFiles() {
  auto dir = RuntimeFiles::GetDirectory();
  if (!std::filesystem::exists(dir)) {
    return;
  }

  while (true) {
    try {
      std::filesystem::remove_all(dir);
      return;
    } catch (const std::filesystem::filesystem_error& error) {
      const auto result = MessageBoxW(
        NULL,
        std::format(
          _(L"There was an error while uninstalling helper files; close any "
            L"games that you use with OpenKneebaord, then retry.\n\n"
            L"If you cancel, OpenKneeboard will be uninstalled, but there will "
            L"be files left over in {}, which you can delete later.\n\n"
            L"Error {} ({:#08x}):\n{}"),
          dir.wstring(),
          error.code().value(),
          std::bit_cast<uint32_t>(error.code().value()),
          winrt::to_hstring(error.what()))
          .c_str(),
        L"Uninstall OpenKneeboard",
        MB_ICONWARNING | MB_RETRYCANCEL | MB_SYSTEMMODAL);
      if (result == IDCANCEL) {
        return;
      }
    }
  }
}

static void dprint(std::wstring_view message) {
  static std::wstring sExe;
  if (sExe.empty()) {
    wchar_t buffer[MAX_PATH];
    auto size = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    sExe = std::filesystem::path(std::wstring_view {buffer, size}).stem();
  }
  OutputDebugStringW(std::format(L"{}: {}", sExe, message).c_str());
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  dprint(L"Starting up...");
  DisableOpenKneeboardOpenXRLayers(HKEY_LOCAL_MACHINE);
  dprint(L"Disabled HKLM OpenXR layers.");
  DisableOpenKneeboardOpenXRLayers(HKEY_CURRENT_USER);
  dprint(L"Disabled HKCU OpenXR layers.");
  RemoveRuntimeFiles();
  dprint(L"Removed runtime files.");

  return 0;
}
