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
#include <OpenKneeboard/dprint.h>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <shims/winrt.h>

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

void LaunchAndWaitForOpenXRHelperSubprocess(
  RunAs runas,
  std::wstring_view command) {
  auto layerPath = RuntimeFiles::GetDirectory().wstring();
  auto exePath = (RuntimeFiles::GetDirectory()
                  / RuntimeFiles::OPENXR_REGISTER_LAYER_HELPER)
                   .wstring();

  auto commandLine = fmt::format(L"{} \"{}\"", command, layerPath);

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
    return;
  }

  if (!shellExecuteInfo.hProcess) {
    dprint("No process handle");
    OPENKNEEBOARD_BREAK;
    return;
  }

  winrt::handle handle {shellExecuteInfo.hProcess};
  dprint("Waiting for OpenXR helper...");
  WaitForSingleObject(handle.get(), INFINITE);
  dprint("OpenXR helper returned.");
}

void SetOpenXRModeWithHelperProcessImpl(
  OpenXRMode mode,
  std::optional<OpenXRMode> oldMode) {
  if (oldMode && mode != *oldMode) {
    switch (*oldMode) {
      case OpenXRMode::Disabled:
        break;
      case OpenXRMode::CurrentUser:
        LaunchAndWaitForOpenXRHelperSubprocess(
          RunAs::CurrentUser, L"disable-HKCU");
        break;
      case OpenXRMode::AllUsers:
        LaunchAndWaitForOpenXRHelperSubprocess(
          RunAs::Administrator, L"disable-HKLM");
        break;
    }
  }

  switch (mode) {
    case OpenXRMode::Disabled:
      break;
    case OpenXRMode::CurrentUser:
      LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::CurrentUser, L"enable-HKCU");
      break;
    case OpenXRMode::AllUsers:
      LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::Administrator, L"enable-HKLM");
      break;
  }
}

}// namespace

void SetOpenXRModeWithHelperProcess(
  OpenXRMode mode,
  std::optional<OpenXRMode> oldMode) {
  // If we're disabling in HKLM and enabling in HKCU, we want to
  // run the elevated process first, but don't want to block the UI thread on
  // UAC, so use a background thread.
  std::thread([=]() {
    SetOpenXRModeWithHelperProcessImpl(mode, oldMode);
  }).detach();
}

}// namespace OpenKneeboard
