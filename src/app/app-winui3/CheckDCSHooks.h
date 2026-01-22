// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "FilePicker.h"

#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <winrt/microsoft.ui.xaml.h>
#include <winrt/windows.foundation.h>

#include <filesystem>

namespace OpenKneeboard {

task<void> CheckAllDCSHooks(winrt::Microsoft::UI::Xaml::XamlRoot root);

task<void> CheckDCSHooks(
  winrt::Microsoft::UI::Xaml::XamlRoot root,
  std::filesystem::path savedGamesPath);

enum class DCSSavedGamesSelectionTrigger {
  /** User has not explicitly asked to choose a saved games location,
   * but we couldn't infer it.
   *
   * For example, when adding DCS, or on first run. Explain first
   * before asking them to pick a folder.
   */
  IMPLICIT,
  /** User has explicitly asked to pick a folder, no need to explain */
  EXPLICIT,
};

task<std::optional<std::filesystem::path>> ChooseDCSSavedGamesFolder(
  winrt::Microsoft::UI::Xaml::XamlRoot xamlRoot,
  DCSSavedGamesSelectionTrigger trigger);

}// namespace OpenKneeboard
