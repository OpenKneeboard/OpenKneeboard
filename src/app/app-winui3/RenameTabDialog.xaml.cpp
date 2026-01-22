// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "RenameTabDialog.xaml.h"
#include "RenameTabDialog.g.cpp"
// clang-format on

#include <OpenKneeboard/utf8.hpp>

#include <format>

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

RenameTabDialog::RenameTabDialog() {
  InitializeComponent();
  mPrompt = _(L"What would you like to rename this tab to?");
}

RenameTabDialog::~RenameTabDialog() = default;

winrt::hstring RenameTabDialog::TabTitle() noexcept {
  return TitleTextBox().Text();
}

void RenameTabDialog::TabTitle(const winrt::hstring& title) noexcept {
  auto textBox = this->TitleTextBox();
  textBox.Text(title);
  textBox.PlaceholderText(title);
  textBox.Focus(FocusState::Programmatic);
  textBox.SelectAll();
  this->Title(
    box_value(std::format(_(L"Rename '{}'"), std::wstring_view {title})));
}

winrt::hstring RenameTabDialog::Prompt() const noexcept { return mPrompt; }

void RenameTabDialog::Prompt(const winrt::hstring& prompt) noexcept {
  mPrompt = prompt;
  this->mPropertyChangedEvent(
    *this,
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L"Prompt"));
}

}// namespace winrt::OpenKneeboardApp::implementation
