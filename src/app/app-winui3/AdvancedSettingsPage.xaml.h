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
// clang-format on

#include "AdvancedSettingsPage.g.h"
#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.h>

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct AdvancedSettingsPage
  : AdvancedSettingsPageT<AdvancedSettingsPage>,
    OpenKneeboard::WithPropertyChangedEventOnProfileChange<
      AdvancedSettingsPage> {
  AdvancedSettingsPage();
  ~AdvancedSettingsPage();

  bool DualKneeboards() const noexcept;
  void DualKneeboards(bool value) noexcept;

  bool MultipleProfiles() const noexcept;
  void MultipleProfiles(bool value) noexcept;

  bool Bookmarks() const noexcept;
  void Bookmarks(bool value) noexcept;

  bool EnableMouseButtonBindings() const noexcept;
  void EnableMouseButtonBindings(bool value) noexcept;

  bool GazeInputFocus() const noexcept;
  void GazeInputFocus(bool value) noexcept;

  bool LoopPages() const noexcept;
  void LoopPages(bool) noexcept;

  bool LoopTabs() const noexcept;
  void LoopTabs(bool) noexcept;

  bool LoopProfiles() const noexcept;
  void LoopProfiles(bool) noexcept;

  bool LoopBookmarks() const noexcept;
  void LoopBookmarks(bool) noexcept;

  bool InGameHeader() const noexcept;
  void InGameHeader(bool) noexcept;

  bool InGameFooter() const noexcept;
  void InGameFooter(bool) noexcept;

  bool InGameFooterFrameCount() const noexcept;
  void InGameFooterFrameCount(bool) noexcept;

  void RestoreDoodleDefaults(const IInspectable&, const IInspectable&) noexcept;
  void RestoreTextDefaults(const IInspectable&, const IInspectable&) noexcept;
  void RestoreQuirkDefaults(const IInspectable&, const IInspectable&) noexcept;

  uint32_t MinimumPenRadius();
  void MinimumPenRadius(uint32_t value);
  uint32_t PenSensitivity();
  void PenSensitivity(uint32_t value);

  uint32_t MinimumEraseRadius();
  void MinimumEraseRadius(uint32_t value);
  uint32_t EraseSensitivity();
  void EraseSensitivity(uint32_t value);

  float TextPageFontSize();
  void TextPageFontSize(float value);

  bool TintEnabled();
  void TintEnabled(bool value);
  float TintBrightness();
  void TintBrightness(float value);
  winrt::Windows::UI::Color Tint();
  void Tint(winrt::Windows::UI::Color value);

  bool Quirk_OculusSDK_DiscardDepthInformation() const noexcept;
  void Quirk_OculusSDK_DiscardDepthInformation(bool value) noexcept;

  bool Quirk_Varjo_OpenXR_D3D12_DoubleBuffer() const noexcept;
  void Quirk_Varjo_OpenXR_D3D12_DoubleBuffer(bool value) noexcept;

  bool Quirk_OpenXR_AlwaysUpdateSwapchain() const noexcept;
  void Quirk_OpenXR_AlwaysUpdateSwapchain(bool value) noexcept;

  bool CanChangeElevation() const noexcept;

  int32_t DesiredElevation() const noexcept;
  fire_and_forget DesiredElevation(int32_t value) noexcept;

 private:
  winrt::apartment_context mUIThread {};
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct AdvancedSettingsPage : AdvancedSettingsPageT<
                                AdvancedSettingsPage,
                                implementation::AdvancedSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
