// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

// clang-format off
#include "pch.h"
#include "CheckDCSHooks.h"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/FilesDiffer.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>

#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <microsoft.ui.xaml.window.h>

#include <format>
#include <fstream>
#include <set>

#include <shobjidl.h>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt;

namespace OpenKneeboard {

std::string GetLuaContent() {
  const auto runtimeDir = Filesystem::GetRuntimeDirectory();
  const auto rootDir = runtimeDir.parent_path();
  return std::format(
    R"EOF(--[[ OpenKneeboard Hook - @{} ]]--
local okb_path = "{}"
--[[ Path for DLL ]]--
package.cpath = okb_path.."/{}/?.dll;"..package.cpath
--[[ Load the actual LUA hook; ignore failures (e.g. OKB uninstalled) ]]--
pcall(dofile, okb_path.."/{}")
)EOF",
    "generated",
    weakly_canonical(rootDir).generic_string(),
    proximate(
      (runtimeDir / RuntimeFiles::DCSWORLD_HOOK_DLL).parent_path(), rootDir)
      .generic_string(),
    proximate(runtimeDir / RuntimeFiles::DCSWORLD_HOOK_LUA, rootDir)
      .generic_string());
}

enum DCSHookInstallState { UP_TO_DATE, OUT_OF_DATE, NOT_INSTALLED };

static DCSHookInstallState GetHookInstallState(
  const std::filesystem::path& hooksDir) {
  if (!is_directory(hooksDir)) {
    return NOT_INSTALLED;
  }

  const auto luaDest = hooksDir
    / std::filesystem::path(RuntimeFiles::DCSWORLD_HOOK_LUA).filename();

  if (!exists(luaDest)) {
    return NOT_INSTALLED;
  }

  const auto luaContent = GetLuaContent();
  if (luaContent.size() != file_size(luaDest)) {
    return OUT_OF_DATE;
  }

  std::ifstream f(luaDest, std::ios::binary);
  if (std::ranges::equal(
        std::views::istream<char>(f >> std::noskipws), luaContent)) {
    return UP_TO_DATE;
  }

  return OUT_OF_DATE;
}

task<void> CheckDCSHooks(XamlRoot root, std::filesystem::path savedGamesPath) {
  std::error_code ec;
  if (!std::filesystem::exists(savedGamesPath, ec)) {
    // For example, junctions may have a pass traversal error:
    // https://github.com/OpenKneeboard/OpenKneeboard/issues/681
    if (ec) {
      dprint.Warning(
        "Failed to check if DCS saved games path `{}` exists: {} ({})",
        savedGamesPath,
        ec.message(),
        ec.value());
    }
    co_return;
  }

  const auto hooksDir = savedGamesPath / "Scripts" / "Hooks";
  const auto state = GetHookInstallState(hooksDir);

  if (state == UP_TO_DATE) {
    co_return;
  }

  const auto luaDest = hooksDir
    / std::filesystem::path(RuntimeFiles::DCSWORLD_HOOK_LUA).filename();

  ContentDialog dialog;
  dialog.XamlRoot(root);
  dialog.Title(winrt::box_value(winrt::to_hstring(_("DCS Hooks"))));
  dialog.DefaultButton(ContentDialogButton::Primary);
  dialog.PrimaryButtonText(winrt::to_hstring(_("Retry")));
  dialog.CloseButtonText(winrt::to_hstring(_("Ignore")));

  do {
    if (ec) {
      auto result = co_await dialog.ShowAsync();
      if (result != ContentDialogResult::Primary) {
        break;
      }
    }

    if (!(std::filesystem::is_directory(hooksDir)
          || std::filesystem::create_directories(hooksDir, ec))) {
      dialog.Content(
        winrt::box_value(
          winrt::to_hstring(
            std::format(
              _("Failed to create {}: {} ({:#x})"),
              to_utf8(hooksDir),
              ec.message(),
              ec.value()))));
      continue;
    }

    try {
      const auto luaContent = GetLuaContent();
      std::ofstream f(luaDest, std::ios::binary);
      f.exceptions(std::ios::failbit | std::ios::badbit);
      f << luaContent;
    } catch (const std::exception& e) {
      dialog.Content(
        winrt::box_value(
          winrt::to_hstring(
            std::format(
              _("Failed to write to {}: {} - if DCS is running, "
                "close DCS, and try again."),
              to_utf8(luaDest),
              e.what()))));
      dprint.Error("DCS hook copy lua to {} failed: {}", luaDest, e.what());
      continue;
    }

    dprint("✅ Updated DCS Lua hook in {}", savedGamesPath);
    co_return;
  } while (true);
}

task<std::optional<std::filesystem::path>> ChooseDCSSavedGamesFolder(
  winrt::Microsoft::UI::Xaml::XamlRoot xamlRoot,
  DCSSavedGamesSelectionTrigger trigger) {
  if (trigger == DCSSavedGamesSelectionTrigger::IMPLICIT) {
    ContentDialog dialog;
    dialog.XamlRoot(xamlRoot);
    dialog.Title(box_value(to_hstring(_("DCS Saved Games Location"))));
    dialog.Content(box_value(to_hstring(
      _("We couldn't find your DCS saved games folder; "
        "would you like to set it now? "
        "This is required for the DCS tabs to work."))));
    dialog.PrimaryButtonText(to_hstring(_("Choose Saved Games Folder")));
    dialog.CloseButtonText(to_hstring(_("Not Now")));
    dialog.DefaultButton(ContentDialogButton::Primary);

    if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
      co_return std::nullopt;
    }
  }

  constexpr winrt::guid thisCall {
    0xa6605cee,
    0x16ef,
    0x4bbb,
    {0x8d, 0x80, 0xf5, 0x73, 0xac, 0x5b, 0x0c, 0x95}};
  FilePicker picker(gMainWindow);
  picker.SettingsIdentifier(thisCall);
  picker.SuggestedStartLocation(FOLDERID_SavedGames);
  const auto folder = picker.PickSingleFolder();

  if (!folder) {
    co_return std::nullopt;
  }

  co_return *folder;
}

task<void> CheckAllDCSHooks(XamlRoot root) try {
  const winrt::apartment_context uiThread;

  const auto savedGames = Filesystem::GetKnownFolderPath<FOLDERID_SavedGames>();

  const auto kneeboard = gKneeboard.lock();
  for (auto&& game: std::filesystem::directory_iterator(savedGames)) {
    if (!game.is_directory()) {
      continue;
    }
    const auto name = game.path().filename().string();
    if (name == "DCS" || name.starts_with("DCS.")) {
      co_await CheckDCSHooks(root, game.path());
    }
  }
} catch (const std::filesystem::filesystem_error& e) {
  dprint.Error("Failed to check DCS hooks: {}", e.what());
  co_return;
}

}// namespace OpenKneeboard
