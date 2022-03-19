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

// clang-format off
#include "pch.h"
#include "CheckDCSHooks.h"
// clang-format on

#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/utf8.h>
#include <fmt/format.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>
#include <winrt/microsoft.ui.xaml.controls.h>
#include <winrt/windows.storage.pickers.h>

#include <algorithm>

#include "Globals.h"

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt;

namespace OpenKneeboard {

static bool FilesDiffer(
  const std::filesystem::path& a,
  const std::filesystem::path& b) {
  const auto size = std::filesystem::file_size(a);
  if (std::filesystem::file_size(b) != size) {
    return true;
  }

  winrt::handle ah {CreateFile(
    a.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL)};
  winrt::handle bh {CreateFile(
    b.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL)};

  winrt::handle afm {
    CreateFileMappingW(ah.get(), nullptr, PAGE_READONLY, 0, 0, nullptr)};
  winrt::handle bfm {
    CreateFileMappingW(bh.get(), nullptr, PAGE_READONLY, 0, 0, nullptr)};

  auto av = reinterpret_cast<std::byte*>(
    MapViewOfFile(afm.get(), FILE_MAP_READ, 0, 0, 0));
  auto bv = reinterpret_cast<std::byte*>(
    MapViewOfFile(bfm.get(), FILE_MAP_READ, 0, 0, 0));

  auto equal = std::equal(av, av + size, bv, bv + size);

  UnmapViewOfFile(av);
  UnmapViewOfFile(bv);

  return !equal;
}

enum DCSHookInstallState { UP_TO_DATE, OUT_OF_DATE, NOT_INSTALLED };

static DCSHookInstallState GetHookInstallState(
  const std::filesystem::path& hooksDir,
  const std::filesystem::path& exeDir) {
  if (!std::filesystem::is_directory(hooksDir)) {
    return NOT_INSTALLED;
  }

  const auto dllDest = hooksDir / RuntimeFiles::DCSWORLD_HOOK_DLL;
  const auto luaDest = hooksDir / RuntimeFiles::DCSWORLD_HOOK_LUA;

  if (!(std::filesystem::is_regular_file(dllDest)
        && std::filesystem::is_regular_file(dllDest))) {
    return NOT_INSTALLED;
  }

  const auto dllSource = exeDir / RuntimeFiles::DCSWORLD_HOOK_DLL;
  const auto luaSource = exeDir / RuntimeFiles::DCSWORLD_HOOK_LUA;

  if (FilesDiffer(dllSource, dllDest) || FilesDiffer(luaSource, luaDest)) {
    return OUT_OF_DATE;
  }

  return UP_TO_DATE;
}

winrt::Windows::Foundation::IAsyncAction CheckDCSHooks(
  const XamlRoot& root,
  const std::filesystem::path& savedGamesPath) {
  const auto hooksDir = savedGamesPath / "Scripts" / "Hooks";
  wchar_t buffer[MAX_PATH];
  auto length = GetModuleFileNameW(NULL, buffer, MAX_PATH);
  const auto exeDir
    = std::filesystem::path(std::wstring_view(buffer, length)).parent_path();

  const auto state = GetHookInstallState(hooksDir, exeDir);

  if (state == UP_TO_DATE) {
    co_return;
  }

  const auto dllSource = exeDir / RuntimeFiles::DCSWORLD_HOOK_DLL;
  const auto luaSource = exeDir / RuntimeFiles::DCSWORLD_HOOK_LUA;
  const auto dllDest = hooksDir / RuntimeFiles::DCSWORLD_HOOK_DLL;
  const auto luaDest = hooksDir / RuntimeFiles::DCSWORLD_HOOK_LUA;

  ContentDialog dialog;
  dialog.XamlRoot(root);
  dialog.Title(winrt::box_value(winrt::to_hstring(_("DCS Hooks"))));
  dialog.CloseButtonText(winrt::to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Primary);

  if (state == NOT_INSTALLED) {
    dialog.Content(winrt::box_value(winrt::to_hstring(fmt::format(
      fmt::runtime("DCS hooks aren't installed in {}; would you like to "
                   "install them now?"),
      to_utf8(savedGamesPath)))));
    dialog.PrimaryButtonText(winrt::to_hstring(_("Install DCS Hooks")));
  } else {
    if (state != OUT_OF_DATE) {
      throw std::logic_error("Unexpected hook state");
    }
    dialog.Content(winrt::box_value(winrt::to_hstring(fmt::format(
      fmt::runtime(
        "Hooks in {} are out of date; would you like to update them now?"),
      to_utf8(savedGamesPath)))));
    dialog.PrimaryButtonText(winrt::to_hstring(_("Update DCS Hooks")));
  }

  do {
    auto result = co_await dialog.ShowAsync();
    if (result != ContentDialogResult::Primary) {
      co_return;
    }
    dialog.PrimaryButtonText(winrt::to_hstring(_("Retry")));

    std::error_code ec;
    if (!(std::filesystem::is_directory(hooksDir)
          || std::filesystem::create_directories(hooksDir, ec))) {
      dialog.Content(winrt::box_value(winrt::to_hstring(fmt::format(
        fmt::runtime(_("Failed to create {}: {} ({:#x}})")),
        to_utf8(hooksDir),
        ec.message(),
        ec.value()))));
      continue;
    }

    if (!std::filesystem::copy_file(
          luaSource,
          luaDest,
          std::filesystem::copy_options::overwrite_existing,
          ec)) {
      dialog.Content(winrt::box_value(winrt::to_hstring(fmt::format(
        fmt::runtime(_("Failed to write to {}: {} ({:#x}) - if DCS is running, "
                       "close DCS, and try again.")),
        to_utf8(luaDest),
        ec.message(),
        ec.value()))));
      continue;
    }

    if (!std::filesystem::copy_file(
          dllSource,
          dllDest,
          std::filesystem::copy_options::overwrite_existing,
          ec)) {
      dialog.Content(winrt::box_value(winrt::to_hstring(fmt::format(
        fmt::runtime(_("Failed to write to {}: {} ({:#x}) - if DCS is running, "
                       "close DCS, and try again.")),
        to_utf8(luaDest),
        ec.message(),
        ec.value()))));
      continue;
    }

    co_return;
  } while (true);
}

winrt::Windows::Foundation::IAsyncOperation<winrt::hstring>
ChooseDCSSavedGamesFolder(
  const winrt::Microsoft::UI::Xaml::XamlRoot& xamlRoot,
  DCSSavedGamesSelectionTrigger trigger) {
  if (trigger == DCSSavedGamesSelectionTrigger::IMPLICIT) {
    ContentDialog dialog;
    dialog.XamlRoot(xamlRoot);
    dialog.Title(box_value(to_hstring(_("DCS Saved Games Location"))));
    dialog.Content(
      box_value(to_hstring(_("We couldn't find your DCS saved games folder; "
                             "would you like to set it now? "
                             "This is required for the DCS tabs to work."))));
    dialog.PrimaryButtonText(to_hstring(_("Choose Saved Games Folder")));
    dialog.CloseButtonText(to_hstring(_("Not Now")));
    dialog.DefaultButton(ContentDialogButton::Primary);

    if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
      co_return {};
    }
  }

  auto picker = Windows::Storage::Pickers::FolderPicker();
  picker.as<IInitializeWithWindow>()->Initialize(gMainWindow);
  picker.SettingsIdentifier(L"chooseDCSSavedGames");
  picker.FileTypeFilter().Append(L"*");
  picker.SuggestedStartLocation(
    Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);

  auto folder = co_await picker.PickSingleFolderAsync();
  if (!folder) {
    co_return {};
  }

  co_return folder.Path();
}

}// namespace OpenKneeboard
