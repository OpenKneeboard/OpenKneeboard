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
#include "NonVRSettingsPage.xaml.h"
#include "NonVRSettingsPage.g.cpp"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/utf8.h>

#include "Globals.h"

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

NonVRSettingsPage::NonVRSettingsPage() {
  this->InitializeComponent();
}

fire_and_forget NonVRSettingsPage::RestoreDefaults(const IInspectable&, const RoutedEventArgs&) {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default non-VR settings?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    return;
  }

  gKneeboard->SetFlatConfig({});

  if (!mPropertyChangedEvent) {
    return;
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardHeightPercent"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardPaddingPixels"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardOpacity"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardHorizontalPlacement"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardVerticalPlacement"));
}

uint8_t NonVRSettingsPage::KneeboardHeightPercent() {
  return gKneeboard->GetFlatConfig().heightPercent;
}

void NonVRSettingsPage::KneeboardHeightPercent(uint8_t value) {
  auto config = gKneeboard->GetFlatConfig();
  config.heightPercent = value;
  gKneeboard->SetFlatConfig(config);
}

uint32_t NonVRSettingsPage::KneeboardPaddingPixels() {
  return gKneeboard->GetFlatConfig().paddingPixels;
}

void NonVRSettingsPage::KneeboardPaddingPixels(uint32_t value) {
  auto config = gKneeboard->GetFlatConfig();
  config.paddingPixels = value;
  gKneeboard->SetFlatConfig(config);
}

float NonVRSettingsPage::KneeboardOpacity() {
  return gKneeboard->GetFlatConfig().opacity;
}

void NonVRSettingsPage::KneeboardOpacity(float value) {
  auto config = gKneeboard->GetFlatConfig();
  config.opacity = value;
  gKneeboard->SetFlatConfig(config);
}

uint8_t NonVRSettingsPage::KneeboardHorizontalPlacement() {
  return static_cast<uint8_t>(gKneeboard->GetFlatConfig().horizontalAlignment);
}

void NonVRSettingsPage::KneeboardHorizontalPlacement(uint8_t value) {
  auto config = gKneeboard->GetFlatConfig();
  config.horizontalAlignment
    = static_cast<FlatConfig::HorizontalAlignment>(value);
  gKneeboard->SetFlatConfig(config);
}

uint8_t NonVRSettingsPage::KneeboardVerticalPlacement() {
  return static_cast<uint8_t>(gKneeboard->GetFlatConfig().verticalAlignment);
}

void NonVRSettingsPage::KneeboardVerticalPlacement(uint8_t value) {
  auto config = gKneeboard->GetFlatConfig();
  config.verticalAlignment = static_cast<FlatConfig::VerticalAlignment>(value);
  gKneeboard->SetFlatConfig(config);
}

winrt::event_token NonVRSettingsPage::PropertyChanged(
  winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
    handler) {
  return mPropertyChangedEvent.add(handler);
}

void NonVRSettingsPage::PropertyChanged(
  winrt::event_token const& token) noexcept {
  mPropertyChangedEvent.remove(token);
}

}// namespace winrt::OpenKneeboardApp::implementation
