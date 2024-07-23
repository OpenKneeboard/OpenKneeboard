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
#include "OKBDeveloperToolsPage.xaml.h"
#include "OKBDeveloperToolsPage.g.cpp"
// clang-format on

using namespace ::OpenKneeboard;

#include "HelpPage.xaml.h"

#include <shims/winrt/base.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace winrt::OpenKneeboardApp::implementation {

static inline void SetClipboardText(std::string_view text) {
  Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(winrt::to_hstring(text));
  Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
}

OKBDeveloperToolsPage::OKBDeveloperToolsPage() {
  InitializeComponent();
}

OKBDeveloperToolsPage::~OKBDeveloperToolsPage() {
}

void OKBDeveloperToolsPage::OnCopyGameEventsClick(
  const IInspectable&,
  const Microsoft::UI::Xaml::RoutedEventArgs&) noexcept {
  SetClipboardText(HelpPage::GetGameEventsAsString());
}

void OKBDeveloperToolsPage::OnCopyDebugMessagesClick(
  const IInspectable&,
  const Microsoft::UI::Xaml::RoutedEventArgs&) noexcept {
  SetClipboardText(winrt::to_string(HelpPage::GetDPrintMessagesAsWString()));
}

}// namespace winrt::OpenKneeboardApp::implementation
