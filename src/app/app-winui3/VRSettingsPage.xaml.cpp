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
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/utf8.h>

#include <cmath>
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
  const RoutedEventArgs&) noexcept {
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
    co_return;
  }

  gKneeboard->ResetVRSettings();

  if (!mPropertyChangedEvent) {
    co_return;
  }

  // Tell the XAML UI elements to update to the new values
  for (auto prop: {
         L"KneeboardX",
         L"KneeboardEyeY",
         L"KneeboardZ",
         L"KneeboardRX",
         L"KneeboardRY",
         L"KneeboardRZ",
         L"KneeboardMaxWidth",
         L"KneeboardMaxHeight",
         L"KneeboardZoomScale",
         L"KneeboardGazeTargetHorizontalScale",
         L"KneeboardGazeTargetVerticalScale",
         L"SteamVREnabled",
         L"GazeZoomEnabled",
         L"NormalOpacity",
         L"GazeOpacity",
       }) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(prop));
  }
}

void VRSettingsPage::RecenterNow(const IInspectable&, const RoutedEventArgs&) {
  gKneeboard->PostUserAction(UserAction::RECENTER_VR);
}

void VRSettingsPage::GoToBindings(const IInspectable&, const RoutedEventArgs&) {
  Frame().Navigate(xaml_typename<InputSettingsPage>());
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
  return gKneeboard->GetVRSettings().mPrimaryLayer.mX;
}

void VRSettingsPage::KneeboardX(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mPrimaryLayer.mX = value;
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardEyeY() {
  return -gKneeboard->GetVRSettings().mPrimaryLayer.mEyeY;
}

void VRSettingsPage::KneeboardEyeY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mPrimaryLayer.mEyeY = -value;
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardZ() {
  // 3D standard right-hand-coordinate system is that -z is forwards;
  // most users expect the opposite.
  return -gKneeboard->GetVRSettings().mPrimaryLayer.mZ;
}

void VRSettingsPage::KneeboardZ(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mPrimaryLayer.mZ = -value;
  gKneeboard->SetVRSettings(config);
}

static inline float RadiansToDegrees(float radians) {
  return radians * 180 / std::numbers::pi_v<float>;
}

static inline float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180;
}

float VRSettingsPage::KneeboardRX() {
  auto raw
    = RadiansToDegrees(gKneeboard->GetVRSettings().mPrimaryLayer.mRX) + 90;
  if (raw < 0) {
    raw += 360.0f;
  }
  if (raw >= 360.0f) {
    raw -= 360.0f;
  }
  return raw <= 180 ? raw : -(360 - raw);
}

void VRSettingsPage::KneeboardRX(float degrees) {
  degrees -= 90;
  if (degrees < 0) {
    degrees += 360;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mPrimaryLayer.mRX
    = DegreesToRadians(degrees <= 180 ? degrees : -(360 - degrees));
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardRY() {
  return -RadiansToDegrees(gKneeboard->GetVRSettings().mPrimaryLayer.mRY);
}

void VRSettingsPage::KneeboardRY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mPrimaryLayer.mRY = -DegreesToRadians(value);
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardRZ() {
  return -RadiansToDegrees(gKneeboard->GetVRSettings().mPrimaryLayer.mRZ);
}

void VRSettingsPage::KneeboardRZ(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mPrimaryLayer.mRZ = -DegreesToRadians(value);
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardMaxHeight() {
  return gKneeboard->GetVRSettings().mMaxHeight;
}

void VRSettingsPage::KneeboardMaxHeight(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mMaxHeight = value;
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardMaxWidth() {
  return gKneeboard->GetVRSettings().mMaxWidth;
}

void VRSettingsPage::KneeboardMaxWidth(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mMaxWidth = value;
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardZoomScale() {
  return gKneeboard->GetVRSettings().mZoomScale;
}

void VRSettingsPage::KneeboardZoomScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mZoomScale = value;
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardGazeTargetHorizontalScale() {
  return gKneeboard->GetVRSettings().mGazeTargetScale.mHorizontal;
}

void VRSettingsPage::KneeboardGazeTargetHorizontalScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mGazeTargetScale.mHorizontal = value;
  gKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardGazeTargetVerticalScale() {
  return gKneeboard->GetVRSettings().mGazeTargetScale.mVertical;
}

void VRSettingsPage::KneeboardGazeTargetVerticalScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRSettings();
  config.mGazeTargetScale.mVertical = value;
  gKneeboard->SetVRSettings(config);
}

uint8_t VRSettingsPage::NormalOpacity() {
  return static_cast<uint8_t>(
    std::lround(gKneeboard->GetVRSettings().mOpacity.mNormal * 100));
}

void VRSettingsPage::NormalOpacity(uint8_t value) {
  auto config = gKneeboard->GetVRSettings();
  config.mOpacity.mNormal = value / 100.0f;
  gKneeboard->SetVRSettings(config);
}

uint8_t VRSettingsPage::GazeOpacity() {
  return static_cast<uint8_t>(
    std::lround(gKneeboard->GetVRSettings().mOpacity.mGaze * 100));
}

void VRSettingsPage::GazeOpacity(uint8_t value) {
  auto config = gKneeboard->GetVRSettings();
  config.mOpacity.mGaze = value / 100.0f;
  gKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::SteamVREnabled() {
  return gKneeboard->GetVRSettings().mEnableSteamVR;
}

void VRSettingsPage::SteamVREnabled(bool enabled) {
  auto config = gKneeboard->GetVRSettings();
  config.mEnableSteamVR = enabled;
  gKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::GazeZoomEnabled() {
  return gKneeboard->GetVRSettings().mEnableGazeZoom;
}

void VRSettingsPage::GazeZoomEnabled(bool enabled) {
  auto config = gKneeboard->GetVRSettings();
  config.mEnableGazeZoom = enabled;
  gKneeboard->SetVRSettings(config);
}

uint8_t VRSettingsPage::OpenXRMode() {
  return static_cast<uint8_t>(gKneeboard->GetVRSettings().mOpenXRMode);
}

void VRSettingsPage::OpenXRMode(uint8_t value) {
  auto config = gKneeboard->GetVRSettings();
  config.mOpenXRMode = static_cast<OpenKneeboard::OpenXRMode>(value);
  gKneeboard->SetVRSettings(config);
}

}// namespace winrt::OpenKneeboardApp::implementation
