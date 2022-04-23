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
  for (auto prop: {
         L"KneeboardX",
         L"KneeboardEyeY",
         L"KneeboardFloorY",
         L"KneeboardZ",
         L"KneeboardRX",
         L"KneeboardRY",
         L"KneeboardRZ",
         L"KneeboardHeight",
         L"KneeboardZoomScale",
         L"KneeboardGazeTargetHorizontalScale",
         L"KneeboardGazeTargetVerticalScale",
         L"SteamVREnabled",
         L"GazeZoomEnabled",
         L"DiscardOculusDepthInformation",
       }) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(prop));
  }
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
  return gKneeboard->GetVRConfig().mX;
}

void VRSettingsPage::KneeboardX(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mX = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardEyeY() {
  return -gKneeboard->GetVRConfig().mEyeY;
}

void VRSettingsPage::KneeboardEyeY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mEyeY = -value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardFloorY() {
  return gKneeboard->GetVRConfig().mFloorY;
}

void VRSettingsPage::KneeboardFloorY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mFloorY = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardZ() {
  // 3D standard right-hand-coordinate system is that -z is forwards;
  // most users expect the opposite.
  return -gKneeboard->GetVRConfig().mZ;
}

void VRSettingsPage::KneeboardZ(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mZ = -value;
  gKneeboard->SetVRConfig(config);
}

static inline float RadiansToDegrees(float radians) {
  return radians * 180 / std::numbers::pi_v<float>;
}

static inline float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180;
}

float VRSettingsPage::KneeboardRX() {
  auto raw = RadiansToDegrees(gKneeboard->GetVRConfig().mRX) + 90;
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
  auto config = gKneeboard->GetVRConfig();
  config.mRX = DegreesToRadians(degrees <= 180 ? degrees : -(360 - degrees));
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardRY() {
  return -RadiansToDegrees(gKneeboard->GetVRConfig().mRY);
}

void VRSettingsPage::KneeboardRY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mRY = -DegreesToRadians(value);
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardRZ() {
  return -RadiansToDegrees(gKneeboard->GetVRConfig().mRZ);
}

void VRSettingsPage::KneeboardRZ(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mRZ = -DegreesToRadians(value);
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardHeight() {
  return gKneeboard->GetVRConfig().mHeight;
}

void VRSettingsPage::KneeboardHeight(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mHeight = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardZoomScale() {
  return gKneeboard->GetVRConfig().mZoomScale;
}

void VRSettingsPage::KneeboardZoomScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mZoomScale = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardGazeTargetHorizontalScale() {
  return gKneeboard->GetVRConfig().mGazeTargetHorizontalScale;
}

void VRSettingsPage::KneeboardGazeTargetHorizontalScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mGazeTargetHorizontalScale = value;
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::KneeboardGazeTargetVerticalScale() {
  return gKneeboard->GetVRConfig().mGazeTargetVerticalScale;
}

void VRSettingsPage::KneeboardGazeTargetVerticalScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = gKneeboard->GetVRConfig();
  config.mGazeTargetVerticalScale = value;
  gKneeboard->SetVRConfig(config);
}

bool VRSettingsPage::DiscardOculusDepthInformation() {
  return (
    gKneeboard->GetVRConfig().mFlags
    & VRRenderConfig::Flags::DISCARD_DEPTH_INFORMATION);
}

void VRSettingsPage::DiscardOculusDepthInformation(bool discard) {
  auto config = gKneeboard->GetVRConfig();
  if (discard) {
    config.mFlags |= VRRenderConfig::Flags::DISCARD_DEPTH_INFORMATION;
  } else {
    config.mFlags &= ~VRRenderConfig::Flags::DISCARD_DEPTH_INFORMATION;
  }
  gKneeboard->SetVRConfig(config);
}

bool VRSettingsPage::SteamVREnabled() {
  return gKneeboard->GetVRConfig().mEnableSteamVR;
}

void VRSettingsPage::SteamVREnabled(bool enabled) {
  auto config = gKneeboard->GetVRConfig();
  config.mEnableSteamVR = enabled;
  gKneeboard->SetVRConfig(config);
}

bool VRSettingsPage::GazeZoomEnabled() {
  return (gKneeboard->GetVRConfig().mFlags & VRRenderConfig::Flags::GAZE_ZOOM);
}

void VRSettingsPage::GazeZoomEnabled(bool enabled) {
  auto config = gKneeboard->GetVRConfig();
  if (enabled) {
    config.mFlags |= VRRenderConfig::Flags::GAZE_ZOOM;
  } else {
    config.mFlags &= ~VRRenderConfig::Flags::GAZE_ZOOM;
  }
  gKneeboard->SetVRConfig(config);
}

uint8_t VRSettingsPage::OpenXRMode() {
  return static_cast<uint8_t>(gKneeboard->GetVRConfig().mOpenXRMode);
}

void VRSettingsPage::OpenXRMode(uint8_t value) {
  auto config = gKneeboard->GetVRConfig();
  config.mOpenXRMode = static_cast<OpenKneeboard::OpenXRMode>(value);
  gKneeboard->SetVRConfig(config);
}

float VRSettingsPage::BaseOpacity() {
  return gKneeboard->GetVRConfig().mBaseOpacity;
}

void VRSettingsPage::BaseOpacity(float value) {
  auto config = gKneeboard->GetVRConfig();
  config.mBaseOpacity = value;
  gKneeboard->SetVRConfig(config);
}

}// namespace winrt::OpenKneeboardApp::implementation
