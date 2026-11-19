// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "VRSettingsPage.xaml.h"
#include "VRSettingsPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/OpenXRMode.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>

using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

static const wchar_t gOpenXRLayerSubkey[] =
  L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit";

VRSettingsPage::VRSettingsPage() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
  this->PopulateViews();
  AddEventListener(
    mKneeboard->evCurrentProfileChangedEvent,
    {get_weak(), &VRSettingsPage::PopulateViews});
  AddEventListener(
    mKneeboard->evSettingsChangedEvent,
    [](auto self) { self->OnVRSettingsChanged(); }
      | bind_refs_front(this) | bind_winrt_context(mUIThread));
}

void VRSettingsPage::OnVRSettingsChanged() noexcept {
  if (!mPropertyChangedEvent) {
    return;
  }
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"PinToHand"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"PinnedHandIndex"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"VREditModeEnabled"));
  mPropertyChangedEvent(
    *this, PropertyChangedEventArgs(L"AllowControllerManipulation"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"IsHeadFollowing"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"IsHandMounted"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadOffsetXCm"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadOffsetYCm"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadDistanceCm"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadPitchDeg"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadYawDeg"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HandOffsetX"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HandOffsetY"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HandOffsetZ"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HandPitchDeg"));
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"WristTiltOffset"));
}

VRSettingsPage::~VRSettingsPage() { this->RemoveAllEventListeners(); }

OpenKneeboard::fire_and_forget VRSettingsPage::RestoreDefaults(
  IInspectable,
  RoutedEventArgs) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(box_value(to_hstring(
    _("Do you want to restore the default VR settings, "
      "removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  co_await mKneeboard->ResetVRSettings();
  co_await mKneeboard->ResetViewsSettings();

  if (!mPropertyChangedEvent) {
    co_return;
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
  this->PopulateViews();
}

bool VRSettingsPage::SteamVREnabled() {
  return mKneeboard->GetVRSettings().mEnableSteamVR;
}

OpenKneeboard::fire_and_forget VRSettingsPage::SteamVREnabled(bool enabled) {
  auto config = mKneeboard->GetVRSettings();
  config.mEnableSteamVR = enabled;
  co_await mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::PinToHand() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mPinToHand;
}

OpenKneeboard::fire_and_forget VRSettingsPage::PinToHand(
  bool enabled) noexcept {
  auto config = mKneeboard->GetVRSettings();
  if (config.mHeadFollow.mPinToHand == enabled) {
    co_return;
  }
  config.mHeadFollow.mPinToHand = enabled;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"PinToHand"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"IsHandMounted"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"IsHeadFollowing"));
  }
}

int32_t VRSettingsPage::PinnedHandIndex() noexcept {
  return static_cast<int32_t>(
    mKneeboard->GetVRSettings().mHeadFollow.mPinnedHand == 1 ? 1 : 0);
}

OpenKneeboard::fire_and_forget VRSettingsPage::PinnedHandIndex(
  int32_t value) noexcept {
  const uint8_t hand = (value == 1) ? 1 : 0;
  auto config = mKneeboard->GetVRSettings();
  if (config.mHeadFollow.mPinnedHand == hand) {
    co_return;
  }
  config.mHeadFollow.mPinnedHand = hand;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"PinnedHandIndex"));
  }
}

bool VRSettingsPage::VREditModeEnabled() noexcept {
  return mKneeboard->GetVRSettings().mEditModeEnabled;
}

OpenKneeboard::fire_and_forget VRSettingsPage::VREditModeEnabled(
  bool enabled) noexcept {
  auto config = mKneeboard->GetVRSettings();
  if (config.mEditModeEnabled == enabled) {
    co_return;
  }
  config.mEditModeEnabled = enabled;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(
      *this, PropertyChangedEventArgs(L"VREditModeEnabled"));
  }
}

bool VRSettingsPage::AllowControllerManipulation() noexcept {
  return mKneeboard->GetVRSettings().mAllowControllerManipulation;
}

OpenKneeboard::fire_and_forget VRSettingsPage::AllowControllerManipulation(
  bool enabled) noexcept {
  auto config = mKneeboard->GetVRSettings();
  if (config.mAllowControllerManipulation == enabled) {
    co_return;
  }
  config.mAllowControllerManipulation = enabled;
  co_await mKneeboard->SetVRSettings(config);
}

int32_t VRSettingsPage::FollowModeIndex() noexcept {
  return static_cast<int32_t>(
    mKneeboard->GetVRSettings().mHeadFollow.mMode);
}

OpenKneeboard::fire_and_forget VRSettingsPage::FollowModeIndex(
  int32_t rawValue) noexcept {
  if (rawValue < 0 || rawValue > 5) {
    co_return;
  }
  const auto mode = static_cast<VRFollowMode>(rawValue);
  auto config = mKneeboard->GetVRSettings();
  if (config.mHeadFollow.mMode == mode) {
    co_return;
  }
  config.mHeadFollow.mMode = mode;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"IsHandMounted"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"IsHeadFollowing"));
  }
}

float VRSettingsPage::FollowDeadzoneAngleDeg() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mDeadzoneAngleDeg;
}

OpenKneeboard::fire_and_forget VRSettingsPage::FollowDeadzoneAngleDeg(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, 2.0f, 60.0f);
  if (config.mHeadFollow.mDeadzoneAngleDeg == clamped) {
    co_return;
  }
  config.mHeadFollow.mDeadzoneAngleDeg = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::FollowDeadzoneDistanceM() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mDeadzoneDistanceM;
}

OpenKneeboard::fire_and_forget VRSettingsPage::FollowDeadzoneDistanceM(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, 0.01f, 0.50f);
  if (config.mHeadFollow.mDeadzoneDistanceM == clamped) {
    co_return;
  }
  config.mHeadFollow.mDeadzoneDistanceM = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::FollowSpeed() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mFollowSpeed;
}

OpenKneeboard::fire_and_forget VRSettingsPage::FollowSpeed(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, 1.0f, 30.0f);
  if (config.mHeadFollow.mFollowSpeed == clamped) {
    co_return;
  }
  config.mHeadFollow.mFollowSpeed = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::FollowLockRoll() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mLockRoll;
}

OpenKneeboard::fire_and_forget VRSettingsPage::FollowLockRoll(
  bool enabled) noexcept {
  auto config = mKneeboard->GetVRSettings();
  if (config.mHeadFollow.mLockRoll == enabled) {
    co_return;
  }
  config.mHeadFollow.mLockRoll = enabled;
  co_await mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::FollowLockPitch() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mLockPitch;
}

OpenKneeboard::fire_and_forget VRSettingsPage::FollowLockPitch(
  bool enabled) noexcept {
  auto config = mKneeboard->GetVRSettings();
  if (config.mHeadFollow.mLockPitch == enabled) {
    co_return;
  }
  config.mHeadFollow.mLockPitch = enabled;
  co_await mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::IsHeadFollowing() noexcept {
  const auto mode =
    GetEffectiveFollowMode(mKneeboard->GetVRSettings().mHeadFollow);
  return mode == VRFollowMode::HeadLocked
    || mode == VRFollowMode::HeadFollowSmooth
    || mode == VRFollowMode::HeadFollowLazy;
}

float VRSettingsPage::HeadOffsetXCm() noexcept {
  return std::round(
    mKneeboard->GetVRSettings().mHeadFollow.mHeadOffsetX * 200.0f) / 2.0f;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HeadOffsetXCm(
  float valueCm) noexcept {
  if (std::isnan(valueCm)) {
    co_return;
  }
  const float meters = std::clamp(valueCm, -80.0f, 80.0f) / 100.0f;
  auto config = mKneeboard->GetVRSettings();
  if (std::abs(config.mHeadFollow.mHeadOffsetX - meters) < 1e-4f) {
    co_return;
  }
  config.mHeadFollow.mHeadOffsetX = meters;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadOffsetXCm"));
  }
}

float VRSettingsPage::HeadOffsetYCm() noexcept {
  return std::round(
    mKneeboard->GetVRSettings().mHeadFollow.mHeadOffsetY * 200.0f) / 2.0f;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HeadOffsetYCm(
  float valueCm) noexcept {
  if (std::isnan(valueCm)) {
    co_return;
  }
  const float meters = std::clamp(valueCm, -60.0f, 60.0f) / 100.0f;
  auto config = mKneeboard->GetVRSettings();
  if (std::abs(config.mHeadFollow.mHeadOffsetY - meters) < 1e-4f) {
    co_return;
  }
  config.mHeadFollow.mHeadOffsetY = meters;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadOffsetYCm"));
  }
}

float VRSettingsPage::HeadDistanceCm() noexcept {
  return std::round(
    -mKneeboard->GetVRSettings().mHeadFollow.mHeadOffsetZ * 200.0f) / 2.0f;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HeadDistanceCm(
  float distanceCm) noexcept {
  if (std::isnan(distanceCm)) {
    co_return;
  }
  const float meters = -std::clamp(distanceCm, 25.0f, 120.0f) / 100.0f;
  auto config = mKneeboard->GetVRSettings();
  if (std::abs(config.mHeadFollow.mHeadOffsetZ - meters) < 1e-4f) {
    co_return;
  }
  config.mHeadFollow.mHeadOffsetZ = meters;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadDistanceCm"));
  }
}

float VRSettingsPage::HeadPitchDeg() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mHeadPitchDeg;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HeadPitchDeg(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  const float clamped = std::clamp(value, -45.0f, 45.0f);
  auto config = mKneeboard->GetVRSettings();
  if (std::abs(config.mHeadFollow.mHeadPitchDeg - clamped) < 1e-3f) {
    co_return;
  }
  config.mHeadFollow.mHeadPitchDeg = clamped;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadPitchDeg"));
  }
}

float VRSettingsPage::HeadYawDeg() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mHeadYawDeg;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HeadYawDeg(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  const float clamped = std::clamp(value, -60.0f, 60.0f);
  auto config = mKneeboard->GetVRSettings();
  if (std::abs(config.mHeadFollow.mHeadYawDeg - clamped) < 1e-3f) {
    co_return;
  }
  config.mHeadFollow.mHeadYawDeg = clamped;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadYawDeg"));
  }
}

OpenKneeboard::fire_and_forget VRSettingsPage::CenterHeadOverlay(
  IInspectable,
  RoutedEventArgs) noexcept {
  auto config = mKneeboard->GetVRSettings();
  config.mHeadFollow.mHeadOffsetX = 0.0f;
  config.mHeadFollow.mHeadOffsetY = 0.0f;
  config.mHeadFollow.mHeadOffsetZ = -0.60f;
  config.mHeadFollow.mHeadPitchDeg = 0.0f;
  config.mHeadFollow.mHeadYawDeg = 0.0f;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadOffsetXCm"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadOffsetYCm"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadDistanceCm"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadPitchDeg"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadYawDeg"));
  }
}

OpenKneeboard::fire_and_forget VRSettingsPage::PresetLowerRightHud(
  IInspectable,
  RoutedEventArgs) noexcept {
  auto config = mKneeboard->GetVRSettings();
  config.mHeadFollow.mHeadOffsetX = 0.10f;
  config.mHeadFollow.mHeadOffsetY = -0.06f;
  config.mHeadFollow.mHeadOffsetZ = -0.58f;
  config.mHeadFollow.mHeadPitchDeg = -6.0f;
  config.mHeadFollow.mHeadYawDeg = -10.0f;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadOffsetXCm"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadOffsetYCm"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadDistanceCm"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadPitchDeg"));
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"HeadYawDeg"));
  }
}

bool VRSettingsPage::IsHandMounted() noexcept {
  const auto mode =
    GetEffectiveFollowMode(mKneeboard->GetVRSettings().mHeadFollow);
  return mode == VRFollowMode::LeftHand || mode == VRFollowMode::RightHand;
}

float VRSettingsPage::HandOffsetX() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mHandOffsetX;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HandOffsetX(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, -0.65f, 0.65f);
  if (config.mHeadFollow.mHandOffsetX == clamped) {
    co_return;
  }
  config.mHeadFollow.mHandOffsetX = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::HandOffsetY() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mHandOffsetY;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HandOffsetY(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, -0.45f, 0.65f);
  if (config.mHeadFollow.mHandOffsetY == clamped) {
    co_return;
  }
  config.mHeadFollow.mHandOffsetY = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::HandOffsetZ() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mHandOffsetZ;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HandOffsetZ(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, -0.65f, 0.65f);
  if (config.mHeadFollow.mHandOffsetZ == clamped) {
    co_return;
  }
  config.mHeadFollow.mHandOffsetZ = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::HandPitchDeg() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mHandPitchDeg;
}

OpenKneeboard::fire_and_forget VRSettingsPage::HandPitchDeg(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, -180.0f, 180.0f);
  if (config.mHeadFollow.mHandPitchDeg == clamped) {
    co_return;
  }
  config.mHeadFollow.mHandPitchDeg = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::WristTiltOffset() noexcept {
  return mKneeboard->GetVRSettings().mHeadFollow.mWristTiltOffset;
}

OpenKneeboard::fire_and_forget VRSettingsPage::WristTiltOffset(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, -45.0f, 45.0f);
  if (config.mHeadFollow.mWristTiltOffset == clamped) {
    co_return;
  }
  config.mHeadFollow.mWristTiltOffset = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::GesturesEnabled() noexcept {
  return mKneeboard->GetVRSettings().mGestures.mEnabled;
}

OpenKneeboard::fire_and_forget VRSettingsPage::GesturesEnabled(
  bool enabled) noexcept {
  auto config = mKneeboard->GetVRSettings();
  if (config.mGestures.mEnabled == enabled) {
    co_return;
  }
  config.mGestures.mEnabled = enabled;
  co_await mKneeboard->SetVRSettings(config);
  if (mPropertyChangedEvent) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"GesturesEnabled"));
  }
}

bool VRSettingsPage::GesturesAllowPinchReposition() noexcept {
  return mKneeboard->GetVRSettings().mGestures.mAllowPinchReposition;
}

OpenKneeboard::fire_and_forget VRSettingsPage::GesturesAllowPinchReposition(
  bool enabled) noexcept {
  auto config = mKneeboard->GetVRSettings();
  if (config.mGestures.mAllowPinchReposition == enabled) {
    co_return;
  }
  config.mGestures.mAllowPinchReposition = enabled;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::GesturePinchThresholdMm() noexcept {
  return mKneeboard->GetVRSettings().mGestures.mPinchThresholdM * 1000.0f;
}

OpenKneeboard::fire_and_forget VRSettingsPage::GesturePinchThresholdMm(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const float thresholdM = std::clamp(value, 10.0f, 60.0f) / 1000.0f;
  if (config.mGestures.mPinchThresholdM == thresholdM) {
    co_return;
  }
  config.mGestures.mPinchThresholdM = thresholdM;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::GestureSwipeMinDistanceM() noexcept {
  return mKneeboard->GetVRSettings().mGestures.mSwipeMinDistanceM;
}

OpenKneeboard::fire_and_forget VRSettingsPage::GestureSwipeMinDistanceM(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto clamped = std::clamp(value, 0.05f, 0.50f);
  if (config.mGestures.mSwipeMinDistanceM == clamped) {
    co_return;
  }
  config.mGestures.mSwipeMinDistanceM = clamped;
  co_await mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::GestureCooldownMs() noexcept {
  return static_cast<float>(
    mKneeboard->GetVRSettings().mGestures.mCooldownMs);
}

OpenKneeboard::fire_and_forget VRSettingsPage::GestureCooldownMs(
  float value) noexcept {
  if (std::isnan(value)) {
    co_return;
  }
  auto config = mKneeboard->GetVRSettings();
  const auto ms = static_cast<uint32_t>(
    std::lround(std::clamp(value, 100.0f, 1500.0f)));
  if (config.mGestures.mCooldownMs == ms) {
    co_return;
  }
  config.mGestures.mCooldownMs = ms;
  co_await mKneeboard->SetVRSettings(config);
}

static bool IsOpenXRAPILayerEnabled(
  const std::filesystem::path& jsonPath,
  DWORD extraFlags) {
  DWORD data {};
  DWORD dataSize = sizeof(data);

  const auto result = RegGetValueW(
    HKEY_LOCAL_MACHINE,
    gOpenXRLayerSubkey,
    jsonPath.c_str(),
    RRF_RT_DWORD | extraFlags,
    nullptr,
    &data,
    &dataSize);

  if (result != ERROR_SUCCESS) {
    return false;
  }
  const auto disabled = static_cast<bool>(data);
  return !disabled;
}

bool VRSettingsPage::OpenXR64Enabled() noexcept {
  const auto jsonPath = std::filesystem::canonical(
                          RuntimeFiles::GetInstallationDirectory()
                          / RuntimeFiles::OPENXR_64BIT_JSON)
                          .wstring();
  return IsOpenXRAPILayerEnabled(jsonPath, RRF_SUBKEY_WOW6464KEY);
}

bool VRSettingsPage::OpenXR32Enabled() noexcept {
  const auto jsonPath = std::filesystem::canonical(
                          RuntimeFiles::GetInstallationDirectory()
                          / RuntimeFiles::OPENXR_32BIT_JSON)
                          .wstring();
  return IsOpenXRAPILayerEnabled(jsonPath, RRF_SUBKEY_WOW6432KEY);
}

OpenKneeboard::fire_and_forget VRSettingsPage::AddView(
  muxc::TabView tabView,
  IInspectable) noexcept {
  auto settings = mKneeboard->GetViewsSettings();

  if (settings.mViews.size() >= MaxViewCount) {
    ContentDialog dialog;
    dialog.XamlRoot(XamlRoot());
    dialog.Title(winrt::box_value(winrt::to_hstring(_("Too many views"))));
    dialog.DefaultButton(ContentDialogButton::Close);
    dialog.CloseButtonText(winrt::to_hstring(_("OK")));
    dialog.Content(box_value(to_hstring(
      std::format(
        "OpenKneeboard supports up to {} views; you currently have {}, so "
        " you can't add another. You might also be limited by "
        "your OpenXR runtime.",
        MaxViewCount,
        settings.mViews.size()))));
    co_await dialog.ShowAsync();
    co_return;
  }

  auto kinds = AddViewKind().Items();
  kinds.Clear();

  kinds.Append(IndependentVRViewUIKind {});
  AddViewKind().SelectedIndex(0);

  using Type = ViewVRSettings::Type;

  std::unordered_set<winrt::guid> mirrored;
  std::unordered_map<winrt::guid, uint32_t> indices;

  for (const auto& view: settings.mViews) {
    const auto& vr = view.mVR;
    if (!vr.mEnabled) {
      continue;
    }

    if (vr.GetType() != Type::Independent) {
      mirrored.emplace(vr.GetMirrorOfGUID());
      continue;
    }

    HorizontalMirrorVRViewUIKind item;
    item.MirrorOf(view.mGuid);
    item.Label(
      to_hstring(std::format(_("Horizontal mirror of '{}'"), view.mName)));
    kinds.Append(item);
    indices.emplace(view.mGuid, kinds.Size() - 1);
  }

  for (const auto& view: settings.mViews) {
    if (view.mVR.GetType() != Type::Independent) {
      continue;
    }
    if (mirrored.contains(view.mGuid)) {
      continue;
    }
    if (!indices.contains(view.mGuid)) {
      continue;
    }
    AddViewKind().SelectedIndex(indices.at(view.mGuid));
    break;
  }

  if (co_await AddViewDialog().ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  ViewVRSettings vr;
  vr.mEnabled = true;

  auto item = AddViewKind().SelectedItem();
  if (item.try_as<IndependentVRViewUIKind>()) {
    vr = ViewVRSettings::Independent({});
  } else {
    auto mirror = item.as<HorizontalMirrorVRViewUIKind>();
    vr = ViewVRSettings::HorizontalMirrorOf(mirror.MirrorOf());
  }

  std::string name;
  for (size_t i = settings.mViews.size() + 1; true; ++i) {
    name = std::format(_("Kneeboard {}"), i);
    auto it = std::ranges::find(settings.mViews, name, &ViewSettings::mName);
    if (it == settings.mViews.end()) {
      break;
    }
  }

  if (
    settings.mViews.size() < 2
    && settings.mAppWindowMode == AppWindowViewMode::NoDecision) {
    const auto result = co_await AppWindowViewModeDialog().ShowAsync();
    settings.mAppWindowMode = static_cast<AppWindowViewMode>(
      AppWindowViewModeDialog().SelectedMode(result));
    if (settings.mAppWindowMode == AppWindowViewMode::NoDecision) {
      // User clicked cancel or pressed escape
      co_return;
    }
  }

  settings.mViews.push_back({.mName = name, .mVR = vr});
  co_await mKneeboard->SetViewsSettings(settings);
  AppendViewTab(settings.mViews.back());

  TabView().SelectedIndex(TabView().TabItems().Size() - 1);
}

OpenKneeboard::fire_and_forget VRSettingsPage::RemoveView(
  muxc::TabView tabView,
  muxc::TabViewTabCloseRequestedEventArgs args) noexcept {
  const auto guid = unbox_value<winrt::guid>(args.Tab().Tag());

  {
    const auto views = mKneeboard->GetViewsSettings().mViews;
    const auto it = std::ranges::find(views, guid, &ViewSettings::mGuid);

    const auto message = std::format(
      _("Do you want to completely remove \"{}\" and delete all its' "
        "settings?"),
      it->mName);

    ContentDialog dialog;
    dialog.XamlRoot(XamlRoot());
    dialog.Title(winrt::box_value(winrt::to_hstring(_("Delete view?"))));
    dialog.DefaultButton(ContentDialogButton::Primary);
    dialog.PrimaryButtonText(winrt::to_hstring(_("Delete")));
    dialog.CloseButtonText(winrt::to_hstring(_("Cancel")));
    dialog.Content(box_value(to_hstring(message)));

    if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
      co_return;
    }
  }

  // While it was modal and nothing else 'should' have changed things in the
  // mean time, re-fetch just in case
  auto settings = mKneeboard->GetViewsSettings();
  std::erase_if(settings.mViews, [guid](const ViewSettings& view) {
    if (view.mGuid == guid) {
      return true;
    }
    if (view.mVR.GetType() != ViewVRSettings::Type::HorizontalMirror) {
      return false;
    }
    return view.mVR.GetMirrorOfGUID() == guid;
  });
  co_await mKneeboard->SetViewsSettings(settings);

  auto items = tabView.TabItems();
  uint32_t selectedIndex = tabView.SelectedIndex();

  for (uint32_t i = 0; i < items.Size(); /* no increment */) {
    const auto& item = items.GetAt(i);
    const auto itemView =
      unbox_value<winrt::guid>(item.as<TabViewItem>().Tag());
    const auto it = std::ranges::find(
      settings.mViews, itemView, [](const ViewSettings& view) {
        return view.mGuid;
      });
    if (it == settings.mViews.end()) {
      if (i == selectedIndex) {
        tabView.SelectedIndex(--selectedIndex);
      }
      items.RemoveAt(i);
    } else {
      ++i;
    }
  }
}

void VRSettingsPage::AppendViewTab(const ViewSettings& view) noexcept {
  auto items = TabView().TabItems();
  TabViewItem tab;
  tab.Tag(winrt::box_value(view.mGuid));
  tab.Header(winrt::box_value(winrt::to_hstring(view.mName)));
  tab.IsClosable(items.Size() >= 1);

  VRViewSettingsControl viewSettings;
  viewSettings.ViewID(view.mGuid);
  tab.Content(viewSettings);

  TabView().TabItems().Append(tab);
}

void VRSettingsPage::PopulateViews() noexcept {
  TabView().TabItems().Clear();
  for (const auto& view: mKneeboard->GetViewsSettings().mViews) {
    AppendViewTab(view);
  }
  TabView().SelectedIndex(0);
}

OpenKneeboard::fire_and_forget VRSettingsPage::OpenXR64Enabled(
  bool enabled) noexcept {
  if (enabled == OpenXR64Enabled()) {
    co_return;
  }

  const auto newValue = enabled ? OpenXRMode::AllUsers : OpenXRMode::Disabled;
  co_await SetOpenXR64ModeWithHelperProcess(newValue);
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"OpenXR64Enabled"));
}

OpenKneeboard::fire_and_forget VRSettingsPage::OpenXR32Enabled(
  bool enabled) noexcept {
  if (enabled == OpenXR32Enabled()) {
    co_return;
  }

  const auto newValue = enabled ? OpenXRMode::AllUsers : OpenXRMode::Disabled;
  co_await SetOpenXR32ModeWithHelperProcess(newValue);
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"OpenXR32Enabled"));
}

}// namespace winrt::OpenKneeboardApp::implementation
