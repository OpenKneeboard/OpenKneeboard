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
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/OpenXRMode.hpp>
#include <OpenKneeboard/RunSubprocessAsync.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>

#include <OpenKneeboard/dprint.hpp>

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

winrt::Windows::Foundation::IAsyncAction SetOpenXR64ModeWithHelperProcess(
  OpenXRMode mode) {
  switch (mode) {
    case OpenXRMode::Disabled:
      co_await LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::Administrator, L"disable-HKLM-64");
      break;
    case OpenXRMode::AllUsers:
      co_await LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::Administrator, L"enable-HKLM-64");
      break;
  }
}

winrt::Windows::Foundation::IAsyncAction SetOpenXR32ModeWithHelperProcess(
  OpenXRMode mode) {
  switch (mode) {
    case OpenXRMode::Disabled:
      co_await LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::Administrator, L"disable-HKLM-32");
      break;
    case OpenXRMode::AllUsers:
      co_await LaunchAndWaitForOpenXRHelperSubprocess(
        RunAs::Administrator, L"enable-HKLM-32");
      break;
  }
}

}// namespace OpenKneeboard
