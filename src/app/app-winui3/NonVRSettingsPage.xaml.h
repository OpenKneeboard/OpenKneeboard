// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "NonVRSettingsPage.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/ViewsSettings.hpp>

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

  OpenKneeboard::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

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

  ViewNonVRSettings GetViewConfig();
  OpenKneeboard::fire_and_forget SetViewConfig(ViewNonVRSettings);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct NonVRSettingsPage
  : NonVRSettingsPageT<NonVRSettingsPage, implementation::NonVRSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
