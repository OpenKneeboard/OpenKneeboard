// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "RenameTabDialog.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

namespace winrt::OpenKneeboardApp::implementation {
struct RenameTabDialog : RenameTabDialogT<RenameTabDialog>,
                         OpenKneeboard::WithPropertyChangedEvent {
  RenameTabDialog();
  ~RenameTabDialog();

  winrt::hstring TabTitle() noexcept;
  void TabTitle(const winrt::hstring&) noexcept;

  winrt::hstring Prompt() const noexcept;
  void Prompt(const winrt::hstring&) noexcept;

 private:
  winrt::hstring mPrompt;
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct RenameTabDialog
  : RenameTabDialogT<RenameTabDialog, implementation::RenameTabDialog> {};

}// namespace winrt::OpenKneeboardApp::factory_implementation
