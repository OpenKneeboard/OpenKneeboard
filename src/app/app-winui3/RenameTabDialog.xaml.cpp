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
#include "RenameTabDialog.xaml.h"
#include "RenameTabDialog.g.cpp"
// clang-format on

#include <OpenKneeboard/utf8.h>

#include <format>

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

RenameTabDialog::RenameTabDialog() {
  InitializeComponent();
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

}// namespace winrt::OpenKneeboardApp::implementation
