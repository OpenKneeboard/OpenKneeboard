// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "VRSettingsPage.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ViewsSettings.hpp>

#include <cstdint>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

namespace OpenKneeboard {
class KneeboardState;
}

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {
struct VRSettingsPage
  : VRSettingsPageT<VRSettingsPage>,
    WithPropertyChangedEventOnProfileChange<VRSettingsPage> {
  VRSettingsPage();
  ~VRSettingsPage();

  bool SteamVREnabled();
  OpenKneeboard::fire_and_forget SteamVREnabled(bool);

  bool OpenXR64Enabled() noexcept;
  OpenKneeboard::fire_and_forget OpenXR64Enabled(bool) noexcept;

  bool OpenXR32Enabled() noexcept;
  OpenKneeboard::fire_and_forget OpenXR32Enabled(bool) noexcept;

  bool PinToHand() noexcept;
  OpenKneeboard::fire_and_forget PinToHand(bool) noexcept;
  int32_t PinnedHandIndex() noexcept;
  OpenKneeboard::fire_and_forget PinnedHandIndex(int32_t) noexcept;
  bool VREditModeEnabled() noexcept;
  OpenKneeboard::fire_and_forget VREditModeEnabled(bool) noexcept;
  bool AllowControllerManipulation() noexcept;
  OpenKneeboard::fire_and_forget AllowControllerManipulation(bool) noexcept;

  int32_t FollowModeIndex() noexcept;
  OpenKneeboard::fire_and_forget FollowModeIndex(int32_t) noexcept;
  float FollowDeadzoneAngleDeg() noexcept;
  OpenKneeboard::fire_and_forget FollowDeadzoneAngleDeg(float) noexcept;
  float FollowDeadzoneDistanceM() noexcept;
  OpenKneeboard::fire_and_forget FollowDeadzoneDistanceM(float) noexcept;
  float FollowSpeed() noexcept;
  OpenKneeboard::fire_and_forget FollowSpeed(float) noexcept;
  bool FollowLockRoll() noexcept;
  OpenKneeboard::fire_and_forget FollowLockRoll(bool) noexcept;
  bool FollowLockPitch() noexcept;
  OpenKneeboard::fire_and_forget FollowLockPitch(bool) noexcept;

  bool IsHeadFollowing() noexcept;
  float HeadOffsetXCm() noexcept;
  OpenKneeboard::fire_and_forget HeadOffsetXCm(float) noexcept;
  float HeadOffsetYCm() noexcept;
  OpenKneeboard::fire_and_forget HeadOffsetYCm(float) noexcept;
  float HeadDistanceCm() noexcept;
  OpenKneeboard::fire_and_forget HeadDistanceCm(float) noexcept;
  float HeadPitchDeg() noexcept;
  OpenKneeboard::fire_and_forget HeadPitchDeg(float) noexcept;
  float HeadYawDeg() noexcept;
  OpenKneeboard::fire_and_forget HeadYawDeg(float) noexcept;
  OpenKneeboard::fire_and_forget CenterHeadOverlay(
    IInspectable,
    RoutedEventArgs) noexcept;
  OpenKneeboard::fire_and_forget PresetLowerRightHud(
    IInspectable,
    RoutedEventArgs) noexcept;

  bool IsHandMounted() noexcept;
  float HandOffsetX() noexcept;
  OpenKneeboard::fire_and_forget HandOffsetX(float) noexcept;
  float HandOffsetY() noexcept;
  OpenKneeboard::fire_and_forget HandOffsetY(float) noexcept;
  float HandOffsetZ() noexcept;
  OpenKneeboard::fire_and_forget HandOffsetZ(float) noexcept;
  float HandPitchDeg() noexcept;
  OpenKneeboard::fire_and_forget HandPitchDeg(float) noexcept;
  float WristTiltOffset() noexcept;
  OpenKneeboard::fire_and_forget WristTiltOffset(float) noexcept;

  bool GesturesEnabled() noexcept;
  OpenKneeboard::fire_and_forget GesturesEnabled(bool) noexcept;
  bool GesturesAllowPinchReposition() noexcept;
  OpenKneeboard::fire_and_forget GesturesAllowPinchReposition(bool) noexcept;
  float GesturePinchThresholdMm() noexcept;
  OpenKneeboard::fire_and_forget GesturePinchThresholdMm(float) noexcept;
  float GestureSwipeMinDistanceM() noexcept;
  OpenKneeboard::fire_and_forget GestureSwipeMinDistanceM(float) noexcept;
  float GestureCooldownMs() noexcept;
  OpenKneeboard::fire_and_forget GestureCooldownMs(float) noexcept;

  OpenKneeboard::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

  OpenKneeboard::fire_and_forget AddView(muxc::TabView, IInspectable) noexcept;

  OpenKneeboard::fire_and_forget RemoveView(
    muxc::TabView,
    muxc::TabViewTabCloseRequestedEventArgs) noexcept;

 private:
  OpenKneeboard::audited_ptr<KneeboardState> mKneeboard;
  winrt::apartment_context mUIThread;
  void OnVRSettingsChanged() noexcept;
  void PopulateViews() noexcept;

  void AppendViewTab(const ViewSettings& view) noexcept;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct VRSettingsPage
  : VRSettingsPageT<VRSettingsPage, implementation::VRSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
