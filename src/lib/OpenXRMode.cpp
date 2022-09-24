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
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>
#include <shims/winrt/base.h>

#include <format>

// clang-format off
#include <Windows.h>
#include <Psapi.h>
#include <shellapi.h>
// clang-format on

namespace OpenKneeboard {

namespace {
enum class RunAs {
  CurrentUser,
  Administrator,
};

winrt::Windows::Foundation::IAsyncAction LaunchAndWaitForOpenXRHelperSubprocess(
  RunAs runas,
  std::wstring_view command) {
  auto layerPath = RuntimeFiles::GetInstallationDirectory().wstring();
  auto exePath = (Filesystem::GetRuntimeDirectory()
                  / RuntimeFiles::OPENXR_REGISTER_LAYER_HELPER)
                   .wstring();

  auto commandLine = std::format(L"{} \"{}\"", command, layerPath);

  SHELLEXECUTEINFOW shellExecuteInfo {
    .cbSize = sizeof(SHELLEXECUTEINFOW),
    .fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_NO_CONSOLE,
    .lpVerb = runas == RunAs::CurrentUser ? L"open" : L"runas",
    .lpFile = exePath.c_str(),
    .lpParameters = commandLine.c_str(),
  };

  if (!ShellExecuteExW(&shellExecuteInfo)) {
    auto error = GetLastError();
    dprintf("Failed to spawn subprocess: {}", error);
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  if (!shellExecuteInfo.hProcess) {
    dprint("No process handle");
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  winrt::handle handle {shellExecuteInfo.hProcess};
  dprint("Waiting for OpenXR helper...");
  co_await winrt::resume_on_signal(handle.get());
  dprint("OpenXR helper returned.");
}

}// namespace

winrt::Windows::Foundation::IAsyncAction SetOpenXRModeWithHelperProcess(
  OpenXRMode mode,
  std::optional<OpenXRMode> oldMode) {
  if (oldMode && mode != *oldMode) {
    switch (*oldMode) {
      case OpenXRMode::Disabled:
        break;
      case OpenXRMode::CurrentUser:
        co_await LaunchAndWaitForOpenXRHelperSubprocess(
          RunAs::CurrentUser, L"disable-HKCU");
        break;
      case OpenXRMode::AllUsers:
        co_await LaunchAndWaitForOpenXRHelperSubprocess(
          RunAs::Administrator, L"disable-HKLM");
        break;
    }
  }

  switch (mode) {
    case OpenXRMode::Disabled:
      break;
    case OpenXRMode::CurrentUser:
      co_await LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::CurrentUser, L"enable-HKCU");
      break;
    case OpenXRMode::AllUsers:
      co_await LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::Administrator, L"enable-HKLM");
      break;
  }
}

}// namespace OpenKneeboard
