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
#pragma once

#include "FilePicker.h"

#include <shims/winrt/base.h>

#include <winrt/microsoft.ui.xaml.h>
#include <winrt/windows.foundation.h>

#include <OpenKneeboard/task.hpp>

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
