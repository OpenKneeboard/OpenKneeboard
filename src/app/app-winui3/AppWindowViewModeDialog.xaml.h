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
// clang-format off
#include "pch.h"
#include "AppWindowViewModeDialog.g.h"
// clang-format on

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

namespace winrt::OpenKneeboardApp::implementation {
struct AppWindowViewModeDialog
  : AppWindowViewModeDialogT<AppWindowViewModeDialog> {
  AppWindowViewModeDialog();
  ~AppWindowViewModeDialog();

  uint8_t SelectedMode(ContentDialogResult) const noexcept;
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct AppWindowViewModeDialog : AppWindowViewModeDialogT<
                                   AppWindowViewModeDialog,
                                   implementation::AppWindowViewModeDialog> {};

}// namespace winrt::OpenKneeboardApp::factory_implementation
