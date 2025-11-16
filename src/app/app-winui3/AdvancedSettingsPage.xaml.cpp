// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "AdvancedSettingsPage.xaml.h"
#include "AdvancedSettingsPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/AppSettings.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/LaunchURI.hpp>
#include <OpenKneeboard/RunSubprocessAsync.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>
#include <OpenKneeboard/VRSettings.hpp>
#include <OpenKneeboard/ViewsSettings.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

AdvancedSettingsPage::AdvancedSettingsPage() {
  InitializeComponent();
  mKneeboard = gKneeboard.lock();

  AddEventListener(
    mKneeboard->evSettingsChangedEvent,
    [](auto self) {
      self->mPropertyChangedEvent(
        *self, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L""));
    } | bind_refs_front(this)
      | bind_winrt_context(mUIThread));
}

AdvancedSettingsPage::~AdvancedSettingsPage() {
  this->RemoveAllEventListeners();
}

bool AdvancedSettingsPage::Bookmarks() const noexcept {
  return mKneeboard->GetUISettings().mBookmarks.mEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::Bookmarks(
  bool value) noexcept {
  auto s = mKneeboard->GetUISettings();
  s.mBookmarks.mEnabled = value;
  co_await mKneeboard->SetUISettings(s);

  mPropertyChangedEvent(
    *this,
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L"Bookmarks"));
}

uint8_t AdvancedSettingsPage::AppWindowViewMode() const noexcept {
  return static_cast<uint8_t>(mKneeboard->GetViewsSettings().mAppWindowMode);
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::AppWindowViewMode(
  uint8_t rawValue) noexcept {
  const auto value = static_cast<OpenKneeboard::AppWindowViewMode>(rawValue);
  auto views = mKneeboard->GetViewsSettings();
  if (views.mAppWindowMode == value) {
    co_return;
  }
  views.mAppWindowMode = value;
  co_await mKneeboard->SetViewsSettings(views);
}

bool AdvancedSettingsPage::EnableMouseButtonBindings() const noexcept {
  return mKneeboard->GetDirectInputSettings().mEnableMouseButtonBindings;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::EnableMouseButtonBindings(
  bool value) noexcept {
  auto s = mKneeboard->GetDirectInputSettings();
  if (s.mEnableMouseButtonBindings == value) {
    co_return;
  }
  s.mEnableMouseButtonBindings = value;
  co_await mKneeboard->SetDirectInputSettings(s);
}

bool AdvancedSettingsPage::MultipleProfiles() const noexcept {
  return mKneeboard->GetProfileSettings().mEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::MultipleProfiles(
  bool value) noexcept {
  static bool sHaveShownTip = false;
  auto s = mKneeboard->GetProfileSettings();
  if (value && (!s.mEnabled) && !sHaveShownTip) {
    co_await OpenKneeboard::LaunchURI(
      "openkneeboard:///TeachingTips/ProfileSwitcher");
    sHaveShownTip = true;
  }
  s.mEnabled = value;
  co_await mKneeboard->SetProfileSettings(s);

  mPropertyChangedEvent(
    *this,
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(
      L"MultipleProfiles"));
}

bool AdvancedSettingsPage::GazeInputFocus() const noexcept {
  return mKneeboard->GetVRSettings().mEnableGazeInputFocus;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::GazeInputFocus(
  bool enabled) noexcept {
  auto vrc = mKneeboard->GetVRSettings();
  vrc.mEnableGazeInputFocus = enabled;
  co_await mKneeboard->SetVRSettings(vrc);
}

bool AdvancedSettingsPage::LoopPages() const noexcept {
  return mKneeboard->GetUISettings().mLoopPages;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::LoopPages(
  bool value) noexcept {
  auto s = mKneeboard->GetUISettings();
  s.mLoopPages = value;
  co_await mKneeboard->SetUISettings(s);
}

bool AdvancedSettingsPage::LoopTabs() const noexcept {
  return mKneeboard->GetUISettings().mLoopTabs;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::LoopTabs(
  bool value) noexcept {
  auto s = mKneeboard->GetUISettings();
  s.mLoopTabs = value;
  co_await mKneeboard->SetUISettings(s);
}

bool AdvancedSettingsPage::LoopProfiles() const noexcept {
  return mKneeboard->GetProfileSettings().mLoopProfiles;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::LoopProfiles(
  bool value) noexcept {
  auto s = mKneeboard->GetProfileSettings();
  s.mLoopProfiles = value;
  co_await mKneeboard->SetProfileSettings(s);
}

bool AdvancedSettingsPage::LoopBookmarks() const noexcept {
  return mKneeboard->GetUISettings().mBookmarks.mLoop;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::LoopBookmarks(
  bool value) noexcept {
  auto s = mKneeboard->GetUISettings();
  s.mBookmarks.mLoop = value;
  co_await mKneeboard->SetUISettings(s);
}

bool AdvancedSettingsPage::InGameHeader() const noexcept {
  return mKneeboard->GetUISettings().mInGameUI.mHeaderEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::InGameHeader(
  bool value) noexcept {
  auto s = mKneeboard->GetUISettings();
  s.mInGameUI.mHeaderEnabled = value;
  co_await mKneeboard->SetUISettings(s);
}

bool AdvancedSettingsPage::InGameFooter() const noexcept {
  return mKneeboard->GetUISettings().mInGameUI.mFooterEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::InGameFooter(
  bool value) noexcept {
  auto s = mKneeboard->GetUISettings();
  s.mInGameUI.mFooterEnabled = value;
  co_await mKneeboard->SetUISettings(s);
}

bool AdvancedSettingsPage::InGameFooterFrameCount() const noexcept {
  return mKneeboard->GetUISettings().mInGameUI.mFooterFrameCountEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::InGameFooterFrameCount(
  bool value) noexcept {
  auto s = mKneeboard->GetUISettings();
  if (value == s.mInGameUI.mFooterFrameCountEnabled) {
    co_return;
  }
  s.mInGameUI.mFooterFrameCountEnabled = value;
  co_await mKneeboard->SetUISettings(s);
}

uint32_t AdvancedSettingsPage::MinimumPenRadius() {
  return mKneeboard->GetDoodlesSettings().mPen.mMinimumRadius;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::MinimumPenRadius(
  uint32_t value) {
  auto ds = mKneeboard->GetDoodlesSettings();
  ds.mPen.mMinimumRadius = value;
  co_await mKneeboard->SetDoodlesSettings(ds);
}

uint32_t AdvancedSettingsPage::PenSensitivity() {
  return mKneeboard->GetDoodlesSettings().mPen.mSensitivity;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::PenSensitivity(
  uint32_t value) {
  auto ds = mKneeboard->GetDoodlesSettings();
  ds.mPen.mSensitivity = value;
  co_await mKneeboard->SetDoodlesSettings(ds);
}

uint32_t AdvancedSettingsPage::MinimumEraseRadius() {
  return mKneeboard->GetDoodlesSettings().mEraser.mMinimumRadius;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::MinimumEraseRadius(
  uint32_t value) {
  auto ds = mKneeboard->GetDoodlesSettings();
  ds.mEraser.mMinimumRadius = value;
  co_await mKneeboard->SetDoodlesSettings(ds);
}

uint32_t AdvancedSettingsPage::EraseSensitivity() {
  return mKneeboard->GetDoodlesSettings().mEraser.mSensitivity;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::EraseSensitivity(
  uint32_t value) {
  auto ds = mKneeboard->GetDoodlesSettings();
  ds.mEraser.mSensitivity = value;
  co_await mKneeboard->SetDoodlesSettings(ds);
}

float AdvancedSettingsPage::TextPageFontSize() {
  return mKneeboard->GetTextSettings().mFontSize;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::TextPageFontSize(
  float value) {
  auto s = mKneeboard->GetTextSettings();
  s.mFontSize = value;
  co_await mKneeboard->SetTextSettings(s);
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::RestoreTextDefaults(
  Windows::Foundation::IInspectable,
  Windows::Foundation::IInspectable) noexcept {
  co_await mKneeboard->ResetTextSettings();
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::RestoreDoodleDefaults(
  Windows::Foundation::IInspectable,
  Windows::Foundation::IInspectable) noexcept {
  co_await mKneeboard->ResetDoodlesSettings();

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::RestoreQuirkDefaults(
  Windows::Foundation::IInspectable,
  Windows::Foundation::IInspectable) noexcept {
  auto vr = mKneeboard->GetVRSettings();
  vr.mQuirks = {};
  co_await mKneeboard->SetVRSettings(vr);
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

bool AdvancedSettingsPage::Quirk_OculusSDK_DiscardDepthInformation()
  const noexcept {
  return mKneeboard->GetVRSettings().mQuirks.mOculusSDK_DiscardDepthInformation;
}

OpenKneeboard::fire_and_forget
AdvancedSettingsPage::Quirk_OculusSDK_DiscardDepthInformation(
  bool value) noexcept {
  auto vrc = mKneeboard->GetVRSettings();
  auto& vrcValue = vrc.mQuirks.mOculusSDK_DiscardDepthInformation;
  if (value == vrcValue) {
    co_return;
  }
  vrcValue = value;
  co_await mKneeboard->SetVRSettings(vrc);
}

uint8_t AdvancedSettingsPage::Quirk_OpenXR_Upscaling() const noexcept {
  return static_cast<uint8_t>(
    mKneeboard->GetVRSettings().mQuirks.mOpenXR_Upscaling);
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::Quirk_OpenXR_Upscaling(
  uint8_t rawValue) noexcept {
  const auto value = static_cast<VRSettings::Quirks::Upscaling>(rawValue);
  auto vrs = mKneeboard->GetVRSettings();
  auto& vrsValue = vrs.mQuirks.mOpenXR_Upscaling;
  if (vrsValue == value) {
    co_return;
  }
  vrsValue = value;
  co_await mKneeboard->SetVRSettings(vrs);
}

bool AdvancedSettingsPage::TintEnabled() {
  return mKneeboard->GetUISettings().mTint.mEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::TintEnabled(bool value) {
  auto settings = mKneeboard->GetUISettings();
  if (settings.mTint.mEnabled == value) {
    co_return;
  }
  settings.mTint.mEnabled = value;
  co_await mKneeboard->SetUISettings(settings);
}

winrt::Windows::UI::Color AdvancedSettingsPage::Tint() {
  auto tint = mKneeboard->GetUISettings().mTint;
  return {
    .A = 0xff,
    .R = static_cast<uint8_t>(std::lround(tint.mRed * 0xff)),
    .G = static_cast<uint8_t>(std::lround(tint.mGreen * 0xff)),
    .B = static_cast<uint8_t>(std::lround(tint.mBlue * 0xff)),
  };
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::Tint(
  Windows::UI::Color value) {
  auto settings = mKneeboard->GetUISettings();
  const auto originalTint = settings.mTint;
  auto& tint = settings.mTint;
  tint.mRed = (1.0f * value.R) / 0xff;
  tint.mGreen = (1.0f * value.G) / 0xff;
  tint.mBlue = (1.0f * value.B) / 0xff;
  if (tint == originalTint) {
    co_return;
  }
  co_await mKneeboard->SetUISettings(settings);
}

float AdvancedSettingsPage::TintBrightness() {
  return mKneeboard->GetUISettings().mTint.mBrightness * 100.0f;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::TintBrightness(
  float value) {
  auto settings = mKneeboard->GetUISettings();
  if (settings.mTint.mBrightness == value / 100.0f) {
    co_return;
  }
  settings.mTint.mBrightness = value / 100.0f;
  co_await mKneeboard->SetUISettings(settings);
}

}// namespace winrt::OpenKneeboardApp::implementation
