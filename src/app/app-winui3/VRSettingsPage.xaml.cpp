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
#include "VRSettingsPage.xaml.h"
#include "VRSettingsPage.g.cpp"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/utf8.h>

#include <numbers>

#include "Globals.h"

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

VRSettingsPage::VRSettingsPage() {
  this->InitializeComponent();
}

fire_and_forget VRSettingsPage::RestoreDefaults(
  const IInspectable&,
  const RoutedEventArgs&) {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default VR settings, "
                           "removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    return;
  }

  gKneeboard->SetVRConfig({});

  if (!mPropertyChangedEvent) {
    return;
  }

  // Tell the XAML UI elements to update to the new values
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardX"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardEyeY"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardFloorY"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardZ"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardRX"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardRY"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardRZ"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardHeight"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"KneeboardZoomScale"));
  mPropertyChangedEvent(
    *this, PropertyChangedEventArgs(L"KneeboardGazeTargetHorizontalScale"));
  mPropertyChangedEvent(
    *this, PropertyChangedEventArgs(L"KneeboardGazeTargetVerticalScale"));
}

winrt::event_token VRSettingsPage::PropertyChanged(
  winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
    handler) {
  return mPropertyChangedEvent.add(handler);
}

void VRSettingsPage::PropertyChanged(winrt::event_token const& token) noexcept {
  mPropertyChangedEvent.remove(token);
}

float VRSettingsPage::KneeboardX() {
  return gKneeboard->GetVRConfig().x;
}

void VRSettingsPage::KneeboardX(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.x = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardEyeY() {
  return -gKneeboard->GetVRConfig().eyeY;
}

void VRSettingsPage::KneeboardEyeY(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.eyeY = -value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardFloorY() {
  return gKneeboard->GetVRConfig().floorY;
}

void VRSettingsPage::KneeboardFloorY(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.floorY = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardZ() {
  // 3D standard right-hand-coordinate system is that -z is forwards;
  // most users expect the opposite.
  return -gKneeboard->GetVRConfig().z;
}

void VRSettingsPage::KneeboardZ(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.z = -value;
  gKneeboard->SetVRConfig(config);
}

static inline float RadiansToDegrees(float radians) {
  return radians * 180 / std::numbers::pi_v<float>;
}

static inline float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180;
}

float VRSettingsPage::KneeboardRX() {
  return RadiansToDegrees(gKneeboard->GetVRConfig().rx);
}

void VRSettingsPage::KneeboardRX(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.rx = DegreesToRadians(value);
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardRY() {
  return -RadiansToDegrees(gKneeboard->GetVRConfig().ry);
}

void VRSettingsPage::KneeboardRY(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.ry = -DegreesToRadians(value);
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardRZ() {
  return -RadiansToDegrees(gKneeboard->GetVRConfig().rz);
}

void VRSettingsPage::KneeboardRZ(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.rz = -DegreesToRadians(value);
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardHeight() {
  return gKneeboard->GetVRConfig().height;
}

void VRSettingsPage::KneeboardHeight(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.height = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardZoomScale() {
  return gKneeboard->GetVRConfig().zoomScale;
}

void VRSettingsPage::KneeboardZoomScale(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.zoomScale = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardGazeTargetHorizontalScale() {
  return gKneeboard->GetVRConfig().gazeTargetHorizontalScale;
}

void VRSettingsPage::KneeboardGazeTargetHorizontalScale(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.gazeTargetHorizontalScale = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardGazeTargetVerticalScale() {
  return gKneeboard->GetVRConfig().gazeTargetVerticalScale;
}

void VRSettingsPage::KneeboardGazeTargetVerticalScale(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.gazeTargetVerticalScale = value;
  gKneeboard->SetVRConfig(config);
}

}// namespace winrt::OpenKneeboardApp::implementation
