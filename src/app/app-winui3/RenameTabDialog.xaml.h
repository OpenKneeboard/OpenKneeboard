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
