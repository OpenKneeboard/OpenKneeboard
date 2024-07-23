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
#include "NonVRSettingsPage.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/ViewsConfig.hpp>

namespace OpenKneeboard {
class KneeboardState;
}// namespace OpenKneeboard

using namespace winrt::Microsoft::UI::Xaml;
using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {
struct NonVRSettingsPage
  : NonVRSettingsPageT<NonVRSettingsPage>,
    OpenKneeboard::WithPropertyChangedEventOnProfileChange<NonVRSettingsPage> {
  NonVRSettingsPage();

  fire_and_forget RestoreDefaults(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;

  uint8_t KneeboardHeightPercent();
  void KneeboardHeightPercent(uint8_t value);
  uint32_t KneeboardPaddingPixels();
  void KneeboardPaddingPixels(uint32_t value);
  float KneeboardOpacity();
  void KneeboardOpacity(float value);
  uint8_t KneeboardHorizontalPlacement();
  void KneeboardHorizontalPlacement(uint8_t value);
  uint8_t KneeboardVerticalPlacement();
  void KneeboardVerticalPlacement(uint8_t value);

 private:
  std::shared_ptr<KneeboardState> mKneeboard;

  // Still not supporting multiple non-VR views
  const size_t mCurrentView = 0;

  ViewNonVRConfig GetViewConfig();
  void SetViewConfig(const ViewNonVRConfig&);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct NonVRSettingsPage
  : NonVRSettingsPageT<NonVRSettingsPage, implementation::NonVRSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
