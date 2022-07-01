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
#include "VRSettingsPage.g.h"
// clang-format on

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct VRSettingsPage : VRSettingsPageT<VRSettingsPage> {
  VRSettingsPage();

  fire_and_forget RestoreDefaults(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;

  float KneeboardX();
  void KneeboardX(float value);
  float KneeboardEyeY();
  void KneeboardEyeY(float value);
  float KneeboardFloorY();
  void KneeboardFloorY(float value);
  float KneeboardZ();
  void KneeboardZ(float value);
  float KneeboardRX();
  void KneeboardRX(float value);
  float KneeboardRY();
  void KneeboardRY(float value);
  float KneeboardRZ();
  void KneeboardRZ(float value);
  float KneeboardHeight();
  void KneeboardHeight(float value);
  float KneeboardZoomScale();
  void KneeboardZoomScale(float value);
  float KneeboardGazeTargetHorizontalScale();
  void KneeboardGazeTargetHorizontalScale(float value);
  float KneeboardGazeTargetVerticalScale();
  void KneeboardGazeTargetVerticalScale(float value);

  bool SteamVREnabled();
  void SteamVREnabled(bool);
  bool GazeZoomEnabled();
  void GazeZoomEnabled(bool);

  uint8_t OpenXRMode();
  void OpenXRMode(uint8_t);

  bool DiscardOculusDepthInformation();
  void DiscardOculusDepthInformation(bool value);

  bool InvertOpenXRYAxis() const;
  void InvertOpenXRYAxis(bool value);

  uint8_t NormalOpacity();
  void NormalOpacity(uint8_t);
  uint8_t GazeOpacity();
  void GazeOpacity(uint8_t);

  winrt::event_token PropertyChanged(
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
      handler);
  void PropertyChanged(winrt::event_token const& token) noexcept;

 private:
  winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler>
    mPropertyChangedEvent;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct VRSettingsPage
  : VRSettingsPageT<VRSettingsPage, implementation::VRSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
