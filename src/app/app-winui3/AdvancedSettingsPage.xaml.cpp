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

#include <OpenKneeboard/AppSettings.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/VRConfig.h>

#include "Globals.h"

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

AdvancedSettingsPage::AdvancedSettingsPage() {
  InitializeComponent();
}

winrt::event_token AdvancedSettingsPage::PropertyChanged(
  winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
    handler) {
  return mPropertyChangedEvent.add(handler);
}

void AdvancedSettingsPage::PropertyChanged(
  winrt::event_token const& token) noexcept {
  mPropertyChangedEvent.remove(token);
}

bool AdvancedSettingsPage::DualKneeboards() const noexcept {
  return gKneeboard->GetAppSettings().mDualKneeboards;
}

void AdvancedSettingsPage::DualKneeboards(bool value) noexcept {
  auto s = gKneeboard->GetAppSettings();
  s.mDualKneeboards = value;
  gKneeboard->SetAppSettings(s);
}

namespace {

using Flags = VRRenderConfig::Flags;

bool TestFlag(Flags flag) noexcept {
  return static_cast<bool>(gKneeboard->GetVRConfig().mFlags & flag);
}

void SetFlag(Flags flag, bool value) noexcept {
  auto vr = gKneeboard->GetVRConfig();
  if (value) {
    vr.mFlags |= flag;
  } else {
    vr.mFlags &= ~flag;
  }
  gKneeboard->SetVRConfig(vr);
}

}// namespace

bool AdvancedSettingsPage::GazeInputFocus() const noexcept {
  return TestFlag(Flags::GAZE_INPUT_FOCUS);
}

void AdvancedSettingsPage::GazeInputFocus(bool enabled) noexcept {
  SetFlag(Flags::GAZE_INPUT_FOCUS, enabled);
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

uint32_t AdvancedSettingsPage::MinimumPenRadius() {
  return gKneeboard->GetDoodleSettings().mPen.mMinimumRadius;
}

void AdvancedSettingsPage::MinimumPenRadius(uint32_t value) {
  auto ds = gKneeboard->GetDoodleSettings();
  ds.mPen.mMinimumRadius = value;
  gKneeboard->SetDoodleSettings(ds);
}

uint32_t AdvancedSettingsPage::PenSensitivity() {
  return gKneeboard->GetDoodleSettings().mPen.mSensitivity;
}

void AdvancedSettingsPage::PenSensitivity(uint32_t value) {
  auto ds = gKneeboard->GetDoodleSettings();
  ds.mPen.mSensitivity = value;
  gKneeboard->SetDoodleSettings(ds);
}

uint32_t AdvancedSettingsPage::MinimumEraseRadius() {
  return gKneeboard->GetDoodleSettings().mEraser.mMinimumRadius;
}

void AdvancedSettingsPage::MinimumEraseRadius(uint32_t value) {
  auto ds = gKneeboard->GetDoodleSettings();
  ds.mEraser.mMinimumRadius = value;
  gKneeboard->SetDoodleSettings(ds);
}

uint32_t AdvancedSettingsPage::EraseSensitivity() {
  return gKneeboard->GetDoodleSettings().mEraser.mSensitivity;
}

void AdvancedSettingsPage::EraseSensitivity(uint32_t value) {
  auto ds = gKneeboard->GetDoodleSettings();
  ds.mEraser.mSensitivity = value;
  gKneeboard->SetDoodleSettings(ds);
}

void AdvancedSettingsPage::RestoreDoodleDefaults(
  const IInspectable&,
  const IInspectable&) noexcept {
  gKneeboard->SetDoodleSettings({});

  for (auto prop: {
         L"MinimumPenRadius",
         L"PenSensitivity",
         L"MinimumEraseRadius",
         L"EraseSensitivity",
       }) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(prop));
  }
}

void AdvancedSettingsPage::RestoreQuirkDefaults(
  const IInspectable&,
  const IInspectable&) noexcept {
  auto vr = gKneeboard->GetVRConfig();
  vr.mFlags
    |= (Flags::QUIRK_OCULUSSDK_DISCARD_DEPTH_INFORMATION | Flags::QUIRK_VARJO_OPENXR_D3D12_DOUBLE_BUFFER | Flags::QUIRK_VARJO_OPENXR_INVERT_Y_POSITION);
  gKneeboard->SetVRConfig(vr);
  for (auto prop: {
         L"Quirk_OculusSDK_DiscardDepthInformation",
         L"Quirk_Varjo_OpenXR_InvertYPosition",
         L"Quirk_Varjo_OpenXR_D3D12_DoubleBuffer",
       }) {
    mPropertyChangedEvent(*this, PropertyChangedEventArgs(prop));
  }
}

bool AdvancedSettingsPage::Quirk_OculusSDK_DiscardDepthInformation()
  const noexcept {
  return TestFlag(Flags::QUIRK_OCULUSSDK_DISCARD_DEPTH_INFORMATION);
}

void AdvancedSettingsPage::Quirk_OculusSDK_DiscardDepthInformation(
  bool value) noexcept {
  SetFlag(Flags::QUIRK_OCULUSSDK_DISCARD_DEPTH_INFORMATION, value);
}

bool AdvancedSettingsPage::Quirk_Varjo_OpenXR_InvertYPosition() const noexcept {
  return TestFlag(Flags::QUIRK_VARJO_OPENXR_INVERT_Y_POSITION);
}

void AdvancedSettingsPage::Quirk_Varjo_OpenXR_InvertYPosition(
  bool value) noexcept {
  SetFlag(Flags::QUIRK_VARJO_OPENXR_INVERT_Y_POSITION, value);
}

bool AdvancedSettingsPage::Quirk_Varjo_OpenXR_D3D12_DoubleBuffer()
  const noexcept {
  return TestFlag(Flags::QUIRK_VARJO_OPENXR_D3D12_DOUBLE_BUFFER);
}

void AdvancedSettingsPage::Quirk_Varjo_OpenXR_D3D12_DoubleBuffer(
  bool value) noexcept {
  SetFlag(Flags::QUIRK_VARJO_OPENXR_D3D12_DOUBLE_BUFFER, value);
}

}// namespace winrt::OpenKneeboardApp::implementation
