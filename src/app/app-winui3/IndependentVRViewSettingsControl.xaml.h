// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "IndependentVRViewSettingsControl.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ViewsSettings.hpp>

using namespace winrt::Microsoft::UI::Xaml;

namespace OpenKneeboard {
class KneeboardState;
}

namespace winrt::OpenKneeboardApp::implementation {
struct IndependentVRViewSettingsControl
  : IndependentVRViewSettingsControlT<IndependentVRViewSettingsControl>,
    OpenKneeboard::WithPropertyChangedEvent,
    OpenKneeboard::EventReceiver {
  IndependentVRViewSettingsControl();
  ~IndependentVRViewSettingsControl();

  OpenKneeboard::fire_and_forget RestoreDefaults(
    Windows::Foundation::IInspectable,
    RoutedEventArgs) noexcept;

  winrt::guid ViewID();
  void ViewID(const winrt::guid&);

  OpenKneeboard::fire_and_forget RecenterNow(
    Windows::Foundation::IInspectable,
    RoutedEventArgs);
  OpenKneeboard::fire_and_forget GoToBindings(
    const IInspectable&,
    const RoutedEventArgs&);

  float KneeboardX();
  void KneeboardX(float value);
  float KneeboardEyeY();
  void KneeboardEyeY(float value);
  float KneeboardZ();
  void KneeboardZ(float value);
  float KneeboardRX();
  void KneeboardRX(float value);
  float KneeboardRY();
  void KneeboardRY(float value);
  float KneeboardRZ();
  void KneeboardRZ(float value);
  float KneeboardMaxWidth();
  void KneeboardMaxWidth(float value);
  float KneeboardMaxHeight();
  void KneeboardMaxHeight(float value);
  float KneeboardZoomScale();
  void KneeboardZoomScale(float value);
  float KneeboardGazeTargetHorizontalScale();
  void KneeboardGazeTargetHorizontalScale(float value);
  float KneeboardGazeTargetVerticalScale();
  void KneeboardGazeTargetVerticalScale(float value);

  bool HaveRecentered();

  bool IsGazeZoomEnabled();
  void IsGazeZoomEnabled(bool);

  bool IsUIVisible();
  void IsUIVisible(bool);

  uint8_t NormalOpacity();
  void NormalOpacity(uint8_t);
  uint8_t GazeOpacity();
  void GazeOpacity(uint8_t);

 private:
  OpenKneeboard::audited_ptr<OpenKneeboard::KneeboardState> mKneeboard;

  OpenKneeboard::IndependentViewVRSettings GetViewConfig();
  OpenKneeboard::fire_and_forget SetViewConfig(
    OpenKneeboard::IndependentViewVRSettings);

  winrt::guid mViewID;
  bool mHaveRecentered {false};
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct IndependentVRViewSettingsControl
  : IndependentVRViewSettingsControlT<
      IndependentVRViewSettingsControl,
      implementation::IndependentVRViewSettingsControl> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
