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
#include <OpenKneeboard/RunSubprocessAsync.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>

#include <format>

namespace OpenKneeboard {

static auto LaunchAndWaitForOpenXRHelperSubprocess(
  RunAs runas,
  std::wstring_view command) {
  const auto layerPath = RuntimeFiles::GetInstallationDirectory().wstring();
  const auto exePath = (Filesystem::GetRuntimeDirectory()
                        / RuntimeFiles::OPENXR_REGISTER_LAYER_HELPER)
                         .wstring();

  const auto commandLine = std::format(L"{} \"{}\"", command, layerPath);
  return RunSubprocessAsync(exePath, commandLine, runas);
}

winrt::Windows::Foundation::IAsyncAction SetOpenXRModeWithHelperProcess(
  OpenXRMode mode,
  std::optional<OpenXRMode> oldMode) {
  if (oldMode && mode != *oldMode) {
    switch (*oldMode) {
      case OpenXRMode::Disabled:
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
    case OpenXRMode::AllUsers:
      co_await LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::Administrator, L"enable-HKLM");
      break;
  }
}

}// namespace OpenKneeboard
