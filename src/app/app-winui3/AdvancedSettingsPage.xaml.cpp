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
#include "AdvancedSettingsPage.xaml.h"
#include "AdvancedSettingsPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/AppSettings.h>
#include <OpenKneeboard/Elevation.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/RunSubprocessAsync.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/TroubleshootingStore.h>
#include <OpenKneeboard/VRConfig.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/weak_wrap.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

AdvancedSettingsPage::AdvancedSettingsPage() {
  InitializeComponent();

  AddEventListener(
    gKneeboard->evSettingsChangedEvent,
    weak_wrap(this)([](auto self) -> winrt::fire_and_forget {
      co_await self->mUIThread;
      self->mPropertyChangedEvent(
        *self, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L""));
    }));
}

AdvancedSettingsPage::~AdvancedSettingsPage() {
  this->RemoveAllEventListeners();
}

bool AdvancedSettingsPage::DualKneeboards() const noexcept {
  return gKneeboard->GetAppSettings().mDualKneeboards.mEnabled;
}

void AdvancedSettingsPage::DualKneeboards(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  s.mDualKneeboards.mEnabled = value;
  gKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::Bookmarks() const noexcept {
  return gKneeboard->GetAppSettings().mBookmarks.mEnabled;
}

void AdvancedSettingsPage::Bookmarks(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  s.mBookmarks.mEnabled = value;
  gKneeboard->SetAppSettings(s);

  mPropertyChangedEvent(
    *this,
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L"Bookmarks"));
}

bool AdvancedSettingsPage::MultipleProfiles() const noexcept {
  return gKneeboard->GetProfileSettings().mEnabled;
}

void AdvancedSettingsPage::MultipleProfiles(bool value) noexcept {
  static bool sHaveShownTip = false;
  auto s = gKneeboard->GetProfileSettings();
  if (value && (!s.mEnabled) && !sHaveShownTip) {
    OpenKneeboard::LaunchURI("openkneeboard:///TeachingTips/ProfileSwitcher");
    sHaveShownTip = true;
  }
  s.mEnabled = value;
  gKneeboard->SetProfileSettings(s);

  mPropertyChangedEvent(
    *this,
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(
      L"MultipleProfiles"));
}

bool AdvancedSettingsPage::GazeInputFocus() const noexcept {
  return gKneeboard->GetVRSettings().mEnableGazeInputFocus;
}

void AdvancedSettingsPage::GazeInputFocus(bool enabled) noexcept {
  auto vrc = gKneeboard->GetVRSettings();
  vrc.mEnableGazeInputFocus = enabled;
  gKneeboard->SetVRSettings(vrc);
}

bool AdvancedSettingsPage::LoopPages() const noexcept {
  return gKneeboard->GetAppSettings().mLoopPages;
}

void AdvancedSettingsPage::LoopPages(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  s.mLoopPages = value;
  gKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::LoopTabs() const noexcept {
  return gKneeboard->GetAppSettings().mLoopTabs;
}

void AdvancedSettingsPage::LoopTabs(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  s.mLoopTabs = value;
  gKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::LoopProfiles() const noexcept {
  return gKneeboard->GetProfileSettings().mLoopProfiles;
}

void AdvancedSettingsPage::LoopProfiles(bool value) noexcept {
  auto s = gKneeboard->GetProfileSettings();
  s.mLoopProfiles = value;
  gKneeboard->SetProfileSettings(s);
}

bool AdvancedSettingsPage::LoopBookmarks() const noexcept {
  return gKneeboard->GetAppSettings().mBookmarks.mLoop;
}

void AdvancedSettingsPage::LoopBookmarks(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  s.mBookmarks.mLoop = value;
  gKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::InGameHeader() const noexcept {
  return gKneeboard->GetAppSettings().mInGameUI.mHeaderEnabled;
}

void AdvancedSettingsPage::InGameHeader(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  s.mInGameUI.mHeaderEnabled = value;
  gKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::InGameFooter() const noexcept {
  return gKneeboard->GetAppSettings().mInGameUI.mFooterEnabled;
}

void AdvancedSettingsPage::InGameFooter(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  s.mInGameUI.mFooterEnabled = value;
  gKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::InGameFooterFrameCount() const noexcept {
  return gKneeboard->GetAppSettings().mInGameUI.mFooterFrameCountEnabled;
}

void AdvancedSettingsPage::InGameFooterFrameCount(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  if (value == s.mInGameUI.mFooterFrameCountEnabled) {
    return;
  }
  s.mInGameUI.mFooterFrameCountEnabled = value;
  gKneeboard->SetAppSettings(s);
}

uint32_t AdvancedSettingsPage::MinimumPenRadius() {
  return gKneeboard->GetDoodlesSettings().mPen.mMinimumRadius;
}

void AdvancedSettingsPage::MinimumPenRadius(uint32_t value) {
  auto ds = gKneeboard->GetDoodlesSettings();
  ds.mPen.mMinimumRadius = value;
  gKneeboard->SetDoodlesSettings(ds);
}

uint32_t AdvancedSettingsPage::PenSensitivity() {
  return gKneeboard->GetDoodlesSettings().mPen.mSensitivity;
}

void AdvancedSettingsPage::PenSensitivity(uint32_t value) {
  auto ds = gKneeboard->GetDoodlesSettings();
  ds.mPen.mSensitivity = value;
  gKneeboard->SetDoodlesSettings(ds);
}

uint32_t AdvancedSettingsPage::MinimumEraseRadius() {
  return gKneeboard->GetDoodlesSettings().mEraser.mMinimumRadius;
}

void AdvancedSettingsPage::MinimumEraseRadius(uint32_t value) {
  auto ds = gKneeboard->GetDoodlesSettings();
  ds.mEraser.mMinimumRadius = value;
  gKneeboard->SetDoodlesSettings(ds);
}

uint32_t AdvancedSettingsPage::EraseSensitivity() {
  return gKneeboard->GetDoodlesSettings().mEraser.mSensitivity;
}

void AdvancedSettingsPage::EraseSensitivity(uint32_t value) {
  auto ds = gKneeboard->GetDoodlesSettings();
  ds.mEraser.mSensitivity = value;
  gKneeboard->SetDoodlesSettings(ds);
}

void AdvancedSettingsPage::RestoreDoodleDefaults(
  const IInspectable&,
  const IInspectable&) noexcept {
  gKneeboard->ResetDoodlesSettings();

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

void AdvancedSettingsPage::RestoreQuirkDefaults(
  const IInspectable&,
  const IInspectable&) noexcept {
  auto vr = gKneeboard->GetVRSettings();
  vr.mQuirks = {};
  gKneeboard->SetVRSettings(vr);
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

bool AdvancedSettingsPage::Quirk_OculusSDK_DiscardDepthInformation()
  const noexcept {
  return gKneeboard->GetVRSettings().mQuirks.mOculusSDK_DiscardDepthInformation;
}

void AdvancedSettingsPage::Quirk_OculusSDK_DiscardDepthInformation(
  bool value) noexcept {
  auto vrc = gKneeboard->GetVRSettings();
  auto& vrcValue = vrc.mQuirks.mOculusSDK_DiscardDepthInformation;
  if (value == vrcValue) {
    return;
  }
  vrcValue = value;
  gKneeboard->SetVRSettings(vrc);
}

bool AdvancedSettingsPage::Quirk_Varjo_OpenXR_D3D12_DoubleBuffer()
  const noexcept {
  return gKneeboard->GetVRSettings().mQuirks.mVarjo_OpenXR_D3D12_DoubleBuffer;
}

void AdvancedSettingsPage::Quirk_Varjo_OpenXR_D3D12_DoubleBuffer(
  bool value) noexcept {
  auto vrc = gKneeboard->GetVRSettings();
  auto& vrcValue = vrc.mQuirks.mVarjo_OpenXR_D3D12_DoubleBuffer;
  if (value == vrcValue) {
    return;
  }
  vrcValue = value;
  gKneeboard->SetVRSettings(vrc);
}

bool AdvancedSettingsPage::Quirk_OpenXR_AlwaysUpdateSwapchain() const noexcept {
  return gKneeboard->GetVRSettings().mQuirks.mOpenXR_AlwaysUpdateSwapchain;
}

void AdvancedSettingsPage::Quirk_OpenXR_AlwaysUpdateSwapchain(
  bool value) noexcept {
  auto vrc = gKneeboard->GetVRSettings();
  auto& vrcValue = vrc.mQuirks.mOpenXR_AlwaysUpdateSwapchain;
  if (value == vrcValue) {
    return;
  }
  vrcValue = value;
  gKneeboard->SetVRSettings(vrc);
}

bool AdvancedSettingsPage::CanChangeElevation() const noexcept {
  return !IsShellElevated();
}

int32_t AdvancedSettingsPage::DesiredElevation() const noexcept {
  return static_cast<int32_t>(GetDesiredElevation());
}

winrt::fire_and_forget AdvancedSettingsPage::DesiredElevation(
  int32_t value) noexcept {
  if (value == DesiredElevation()) {
    co_return;
  }

  const scope_guard propertyChanged(
    weak_wrap(this)([](auto self) -> winrt::fire_and_forget {
      co_await self->mUIThread;
      self->mPropertyChangedEvent(
        *self, PropertyChangedEventArgs(L"DesiredElevation"));
    }));

  // Always use the helper; while it's not needed, it never hurts, and gives us
  // a single path to act

  const auto path = (Filesystem::GetRuntimeDirectory()
                     / RuntimeFiles::SET_DESIRED_ELEVATION_HELPER)
                      .wstring();

  const auto cmdline = std::format(L"{}", value);
  if (
    co_await RunSubprocessAsync(path, cmdline, RunAs::Administrator)
    != SubprocessResult::Success) {
    co_return;
  }
  if (IsShellElevated()) {
    // The UI should have been disabled
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  const auto desired = GetDesiredElevation();

  const auto isElevated = IsElevated();
  const bool elevate = (!isElevated) && desired == DesiredElevation::Elevated;
  const bool deElevate
    = (isElevated) && desired == DesiredElevation::NotElevated;
  if (!(elevate || deElevate)) {
    co_return;
  }

  co_await mUIThread;
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_(L"Restart OpenKneeboard?"))));
  dialog.Content(
    box_value(to_hstring(_(L"OpenKneeboard needs to be restarted to change "
                           L"elevation. Would you like to restart it now?"))));
  dialog.PrimaryButtonText(_(L"Restart Now"));
  dialog.CloseButtonText(_(L"Later"));
  dialog.DefaultButton(ContentDialogButton::Primary);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  dprint("Tearing down exclusive resources for relaunch");
  gMutex = {};
  gTroubleshootingStore = {};
  co_await gKneeboard->ReleaseExclusiveResources();

  const auto restarted = RelaunchWithDesiredElevation(desired, SW_SHOWNORMAL);
  if (restarted) {
    Application::Current().Exit();
    co_return;
  }

  // Failed to spawn, e.g. UAC deny
  gTroubleshootingStore = TroubleshootingStore::Get();
  dprint("Relaunch failed, coming back up!");
  gMutex
    = winrt::handle {CreateMutexW(nullptr, TRUE, OpenKneeboard::ProjectNameW)};
  gKneeboard->AcquireExclusiveResources();
}

bool AdvancedSettingsPage::TintEnabled() {
  return gKneeboard->GetAppSettings().mTint.mEnabled;
}

void AdvancedSettingsPage::TintEnabled(bool value) {
  auto settings = gKneeboard->GetAppSettings();
  if (settings.mTint.mEnabled == value) {
    return;
  }
  settings.mTint.mEnabled = value;
  gKneeboard->SetAppSettings(settings);
}

winrt::Windows::UI::Color AdvancedSettingsPage::Tint() {
  auto tint = gKneeboard->GetAppSettings().mTint;
  return {
    .A = 0xff,
    .R = static_cast<uint8_t>(std::lround(tint.mRed * 0xff)),
    .G = static_cast<uint8_t>(std::lround(tint.mGreen * 0xff)),
    .B = static_cast<uint8_t>(std::lround(tint.mBlue * 0xff)),
  };
}

void AdvancedSettingsPage::Tint(Windows::UI::Color value) {
  auto settings = gKneeboard->GetAppSettings();
  const auto originalTint = settings.mTint;
  auto& tint = settings.mTint;
  tint.mRed = (1.0f * value.R) / 0xff;
  tint.mGreen = (1.0f * value.G) / 0xff;
  tint.mBlue = (1.0f * value.B) / 0xff;
  if (tint == originalTint) {
    return;
  }
  gKneeboard->SetAppSettings(settings);
}

float AdvancedSettingsPage::TintBrightness() {
  return gKneeboard->GetAppSettings().mTint.mBrightness * 100.0f;
}

void AdvancedSettingsPage::TintBrightness(float value) {
  auto settings = gKneeboard->GetAppSettings();
  if (settings.mTint.mBrightness == value / 100.0f) {
    return;
  }
  settings.mTint.mBrightness = value / 100.0f;
  gKneeboard->SetAppSettings(settings);
}

}// namespace winrt::OpenKneeboardApp::implementation
