// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "IndependentVRViewSettingsControl.xaml.h"
#include "IndependentVRViewSettingsControl.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/LaunchURI.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <cmath>
#include <numbers>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

IndependentVRViewSettingsControl::IndependentVRViewSettingsControl() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
  AddEventListener(
    mKneeboard->evUserActionEvent,
    [weak = this->get_weak()](const UserAction action) {
      if (action != UserAction::RECENTER_VR) {
        return;
      }
      auto self = weak.get();
      if (!self) [[unlikely]] {
        // Should have been unregistered in destructor
        OPENKNEEBOARD_BREAK;
        return;
      }
      self->mHaveRecentered = true;
      self->mPropertyChangedEvent(
        *self,
        winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs {
          L"HaveRecentered"});
    });
}

IndependentVRViewSettingsControl::~IndependentVRViewSettingsControl() {
  this->RemoveAllEventListeners();
}

OpenKneeboard::fire_and_forget
IndependentVRViewSettingsControl::RestoreDefaults(
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

  if (!mPropertyChangedEvent) {
    co_return;
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

IndependentViewVRSettings IndependentVRViewSettingsControl::GetViewConfig() {
  const auto views = mKneeboard->GetViewsSettings().mViews;

  const auto it = std::ranges::find(views, mViewID, &ViewSettings::mGuid);
  if (it == views.end()) [[unlikely]] {
    fatal("Requested view not found");
  }

  return it->mVR.GetIndependentSettings();
}

OpenKneeboard::fire_and_forget IndependentVRViewSettingsControl::SetViewConfig(
  IndependentViewVRSettings config) {
  auto viewsConfig = mKneeboard->GetViewsSettings();
  auto& views = viewsConfig.mViews;
  auto it = std::ranges::find(views, mViewID, &ViewSettings::mGuid);
  if (it == views.end()) [[unlikely]] {
    fatal("Requested view not found");
  }
  it->mVR.SetIndependentSettings(config);
  co_await mKneeboard->SetViewsSettings(viewsConfig);
}

OpenKneeboard::fire_and_forget IndependentVRViewSettingsControl::RecenterNow(
  IInspectable,
  RoutedEventArgs) {
  co_await mKneeboard->PostUserAction(UserAction::RECENTER_VR);
}

OpenKneeboard::fire_and_forget IndependentVRViewSettingsControl::GoToBindings(
  const IInspectable&,
  const RoutedEventArgs&) {
  co_await LaunchURI(SpecialURIs::SettingsInput());
}

float IndependentVRViewSettingsControl::KneeboardX() {
  return this->GetViewConfig().mPose.mX;
}

void IndependentVRViewSettingsControl::KneeboardX(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mPose.mX = value;
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardEyeY() {
  return -this->GetViewConfig().mPose.mEyeY;
}

void IndependentVRViewSettingsControl::KneeboardEyeY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mPose.mEyeY = -value;
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardZ() {
  // 3D standard right-hand-coordinate system is that -z is forwards;
  // most users expect the opposite.
  return -this->GetViewConfig().mPose.mZ;
}

void IndependentVRViewSettingsControl::KneeboardZ(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mPose.mZ = -value;
  this->SetViewConfig(view);
}

static inline float RadiansToDegrees(float radians) {
  return radians * 180 / std::numbers::pi_v<float>;
}

static inline float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180;
}

float IndependentVRViewSettingsControl::KneeboardRX() {
  auto raw = RadiansToDegrees(this->GetViewConfig().mPose.mRX) + 90;
  if (raw < 0) {
    raw += 360.0f;
  }
  if (raw >= 360.0f) {
    raw -= 360.0f;
  }
  return raw <= 180 ? raw : -(360 - raw);
}

void IndependentVRViewSettingsControl::KneeboardRX(float degrees) {
  degrees -= 90;
  if (degrees < 0) {
    degrees += 360;
  }
  auto view = this->GetViewConfig();
  view.mPose.mRX
    = DegreesToRadians(degrees <= 180 ? degrees : -(360 - degrees));
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardRY() {
  return -RadiansToDegrees(this->GetViewConfig().mPose.mRY);
}

void IndependentVRViewSettingsControl::KneeboardRY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mPose.mRY = -DegreesToRadians(value);
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardRZ() {
  return -RadiansToDegrees(this->GetViewConfig().mPose.mRZ);
}

void IndependentVRViewSettingsControl::KneeboardRZ(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mPose.mRZ = -DegreesToRadians(value);
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardMaxHeight() {
  return this->GetViewConfig().mMaximumPhysicalSize.mHeight;
}

void IndependentVRViewSettingsControl::KneeboardMaxHeight(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mMaximumPhysicalSize.mHeight = value;
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardMaxWidth() {
  return this->GetViewConfig().mMaximumPhysicalSize.mWidth;
}

void IndependentVRViewSettingsControl::KneeboardMaxWidth(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mMaximumPhysicalSize.mWidth = value;
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardZoomScale() {
  return this->GetViewConfig().mZoomScale;
}

void IndependentVRViewSettingsControl::KneeboardZoomScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mZoomScale = value;
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardGazeTargetHorizontalScale() {
  return this->GetViewConfig().mGazeTargetScale.mHorizontal;
}

void IndependentVRViewSettingsControl::KneeboardGazeTargetHorizontalScale(
  float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mGazeTargetScale.mHorizontal = value;
  this->SetViewConfig(view);
}

float IndependentVRViewSettingsControl::KneeboardGazeTargetVerticalScale() {
  return this->GetViewConfig().mGazeTargetScale.mVertical;
}

void IndependentVRViewSettingsControl::KneeboardGazeTargetVerticalScale(
  float value) {
  if (std::isnan(value)) {
    return;
  }
  auto view = this->GetViewConfig();
  view.mGazeTargetScale.mVertical = value;
  this->SetViewConfig(view);
}

uint8_t IndependentVRViewSettingsControl::NormalOpacity() {
  return static_cast<uint8_t>(
    std::lround(this->GetViewConfig().mOpacity.mNormal * 100));
}

void IndependentVRViewSettingsControl::NormalOpacity(uint8_t value) {
  auto view = this->GetViewConfig();
  view.mOpacity.mNormal = value / 100.0f;
  this->SetViewConfig(view);
}

uint8_t IndependentVRViewSettingsControl::GazeOpacity() {
  return static_cast<uint8_t>(
    std::lround(this->GetViewConfig().mOpacity.mGaze * 100));
}

void IndependentVRViewSettingsControl::GazeOpacity(uint8_t value) {
  auto view = this->GetViewConfig();
  view.mOpacity.mGaze = value / 100.0f;
  this->SetViewConfig(view);
}

bool IndependentVRViewSettingsControl::HaveRecentered() {
  return mHaveRecentered;
}

bool IndependentVRViewSettingsControl::IsGazeZoomEnabled() {
  return this->GetViewConfig().mEnableGazeZoom;
}

void IndependentVRViewSettingsControl::IsGazeZoomEnabled(bool enabled) {
  auto view = this->GetViewConfig();
  view.mEnableGazeZoom = enabled;
  this->SetViewConfig(view);
}

bool IndependentVRViewSettingsControl::IsUIVisible() {
  return this->GetViewConfig().mDisplayArea != ViewDisplayArea::ContentOnly;
}

void IndependentVRViewSettingsControl::IsUIVisible(bool visible) {
  auto view = this->GetViewConfig();
  if (visible) {
    view.mDisplayArea = ViewDisplayArea::Full;
  } else {
    view.mDisplayArea = ViewDisplayArea::ContentOnly;
  }
  this->SetViewConfig(view);
}

winrt::guid IndependentVRViewSettingsControl::ViewID() {
  return mViewID;
}

void IndependentVRViewSettingsControl::ViewID(const winrt::guid& v) {
  mViewID = v;
}

}// namespace winrt::OpenKneeboardApp::implementation
