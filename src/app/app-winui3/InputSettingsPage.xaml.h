// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
#include "InputSettingsPage.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.hpp>

#include <string>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation::Collections;

namespace OpenKneeboard {
class KneeboardState;
}

namespace winrt::OpenKneeboardApp::implementation {
struct InputSettingsPage
  : InputSettingsPageT<InputSettingsPage>,
    OpenKneeboard::WithPropertyChangedEventOnProfileChange<InputSettingsPage> {
  InputSettingsPage();
  ~InputSettingsPage();

  OpenKneeboard::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

  IVector<IInspectable> Devices() noexcept;
  void OnOrientationChanged(
    const IInspectable&,
    const SelectionChangedEventArgs&);

 private:
  winrt::apartment_context mUIThread;
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct InputSettingsPage
  : InputSettingsPageT<InputSettingsPage, implementation::InputSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
