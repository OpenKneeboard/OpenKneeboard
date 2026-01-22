// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
