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

#include <OpenKneeboard/AppSettings.hpp>
#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/LaunchURI.hpp>
#include <OpenKneeboard/RunSubprocessAsync.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>
#include <OpenKneeboard/VRConfig.hpp>
#include <OpenKneeboard/ViewsConfig.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/utf8.hpp>

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
  return mKneeboard->GetAppSettings().mBookmarks.mEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::Bookmarks(
  bool value) noexcept {
  auto s = mKneeboard->GetAppSettings();
  s.mBookmarks.mEnabled = value;
  co_await mKneeboard->SetAppSettings(s);

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
  return mKneeboard->GetAppSettings().mLoopPages;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::LoopPages(
  bool value) noexcept {
  auto s = mKneeboard->GetAppSettings();
  s.mLoopPages = value;
  co_await mKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::LoopTabs() const noexcept {
  return mKneeboard->GetAppSettings().mLoopTabs;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::LoopTabs(
  bool value) noexcept {
  auto s = mKneeboard->GetAppSettings();
  s.mLoopTabs = value;
  co_await mKneeboard->SetAppSettings(s);
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
  return mKneeboard->GetAppSettings().mBookmarks.mLoop;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::LoopBookmarks(
  bool value) noexcept {
  auto s = mKneeboard->GetAppSettings();
  s.mBookmarks.mLoop = value;
  co_await mKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::InGameHeader() const noexcept {
  return mKneeboard->GetAppSettings().mInGameUI.mHeaderEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::InGameHeader(
  bool value) noexcept {
  auto s = mKneeboard->GetAppSettings();
  s.mInGameUI.mHeaderEnabled = value;
  co_await mKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::InGameFooter() const noexcept {
  return mKneeboard->GetAppSettings().mInGameUI.mFooterEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::InGameFooter(
  bool value) noexcept {
  auto s = mKneeboard->GetAppSettings();
  s.mInGameUI.mFooterEnabled = value;
  co_await mKneeboard->SetAppSettings(s);
}

bool AdvancedSettingsPage::InGameFooterFrameCount() const noexcept {
  return mKneeboard->GetAppSettings().mInGameUI.mFooterFrameCountEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::InGameFooterFrameCount(
  bool value) noexcept {
  auto s = mKneeboard->GetAppSettings();
  if (value == s.mInGameUI.mFooterFrameCountEnabled) {
    co_return;
  }
  s.mInGameUI.mFooterFrameCountEnabled = value;
  co_await mKneeboard->SetAppSettings(s);
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
  const auto value = static_cast<VRConfig::Quirks::Upscaling>(rawValue);
  auto vrs = mKneeboard->GetVRSettings();
  auto& vrsValue = vrs.mQuirks.mOpenXR_Upscaling;
  if (vrsValue == value) {
    co_return;
  }
  vrsValue = value;
  co_await mKneeboard->SetVRSettings(vrs);
}

bool AdvancedSettingsPage::CanChangeElevation() const noexcept {
  return !IsShellElevated();
}

int32_t AdvancedSettingsPage::DesiredElevation() const noexcept {
  return static_cast<int32_t>(GetDesiredElevation());
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::DesiredElevation(
  int32_t value) noexcept {
  if (value == DesiredElevation()) {
    co_return;
  }

  const scope_exit propertyChanged([](auto self) {
    self->mPropertyChangedEvent(
      *self, PropertyChangedEventArgs(L"DesiredElevation"));
  } | bind_refs_front(this) | bind_winrt_context(mUIThread));

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
  co_await mKneeboard->ReleaseExclusiveResources();

  const auto restarted = RelaunchWithDesiredElevation(desired, SW_SHOWNORMAL);
  if (restarted) {
    Application::Current().Exit();
    co_return;
  }

  // Failed to spawn, e.g. UAC deny
  gTroubleshootingStore = TroubleshootingStore::Get();
  dprint("Relaunch failed, coming back up!");
  gMutex = Win32::or_throw::CreateMutex(
    nullptr, TRUE, OpenKneeboard::ProjectReverseDomainW);
  mKneeboard->AcquireExclusiveResources();
}

bool AdvancedSettingsPage::TintEnabled() {
  return mKneeboard->GetAppSettings().mTint.mEnabled;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::TintEnabled(bool value) {
  auto settings = mKneeboard->GetAppSettings();
  if (settings.mTint.mEnabled == value) {
    co_return;
  }
  settings.mTint.mEnabled = value;
  co_await mKneeboard->SetAppSettings(settings);
}

winrt::Windows::UI::Color AdvancedSettingsPage::Tint() {
  auto tint = mKneeboard->GetAppSettings().mTint;
  return {
    .A = 0xff,
    .R = static_cast<uint8_t>(std::lround(tint.mRed * 0xff)),
    .G = static_cast<uint8_t>(std::lround(tint.mGreen * 0xff)),
    .B = static_cast<uint8_t>(std::lround(tint.mBlue * 0xff)),
  };
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::Tint(
  Windows::UI::Color value) {
  auto settings = mKneeboard->GetAppSettings();
  const auto originalTint = settings.mTint;
  auto& tint = settings.mTint;
  tint.mRed = (1.0f * value.R) / 0xff;
  tint.mGreen = (1.0f * value.G) / 0xff;
  tint.mBlue = (1.0f * value.B) / 0xff;
  if (tint == originalTint) {
    co_return;
  }
  co_await mKneeboard->SetAppSettings(settings);
}

float AdvancedSettingsPage::TintBrightness() {
  return mKneeboard->GetAppSettings().mTint.mBrightness * 100.0f;
}

OpenKneeboard::fire_and_forget AdvancedSettingsPage::TintBrightness(
  float value) {
  auto settings = mKneeboard->GetAppSettings();
  if (settings.mTint.mBrightness == value / 100.0f) {
    co_return;
  }
  settings.mTint.mBrightness = value / 100.0f;
  co_await mKneeboard->SetAppSettings(settings);
}

}// namespace winrt::OpenKneeboardApp::implementation
