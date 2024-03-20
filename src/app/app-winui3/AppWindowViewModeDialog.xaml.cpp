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
#include "AppWindowViewModeDialog.xaml.h"
#include "AppWindowViewModeDialog.g.cpp"
// clang-format on

#include <OpenKneeboard/ViewsConfig.h>

#include <OpenKneeboard/utf8.h>

#include <format>

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

AppWindowViewModeDialog::AppWindowViewModeDialog() {
  InitializeComponent();
}

AppWindowViewModeDialog::~AppWindowViewModeDialog() = default;

uint8_t AppWindowViewModeDialog::SelectedMode(
  ContentDialogResult result) const noexcept {
  using OpenKneeboard::AppWindowViewMode;
  switch (result) {
    case ContentDialogResult::Primary:
      return static_cast<uint8_t>(AppWindowViewMode::ActiveView);
    case ContentDialogResult::Secondary:
      return static_cast<uint8_t>(AppWindowViewMode::Independent);
    default:
      return static_cast<uint8_t>(AppWindowViewMode::NoDecision);
  }
}

}// namespace winrt::OpenKneeboardApp::implementation
