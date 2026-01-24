// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "VRViewSettingsControl.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/ViewsSettings.hpp>

namespace OpenKneeboard {
class KneeboardState;
}

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {
struct VRViewSettingsControl : VRViewSettingsControlT<VRViewSettingsControl>,
                               WithPropertyChangedEvent,
                               EventReceiver {
  VRViewSettingsControl();
  ~VRViewSettingsControl();

  winrt::guid ViewID();
  void ViewID(const winrt::guid&);

  bool IsEnabledInVR();
  OpenKneeboard::fire_and_forget IsEnabledInVR(bool);

  IInspectable SelectedKind();
  OpenKneeboard::fire_and_forget SelectedKind(
    Windows::Foundation::IInspectable);

  IInspectable SelectedDefaultTab();
  OpenKneeboard::fire_and_forget SelectedDefaultTab(
    Windows::Foundation::IInspectable);

  winrt::Microsoft::UI::Xaml::Visibility TooManyViewsVisibility();

 private:
  audited_ptr<OpenKneeboard::KneeboardState> mKneeboard;

  winrt::guid mViewID;

  void PopulateKind(const ViewVRSettings&);
  void PopulateSubcontrol(const ViewVRSettings&);
  void PopulateDefaultTab();

  Control mSubControl {nullptr};
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct VRViewSettingsControl : VRViewSettingsControlT<
                                 VRViewSettingsControl,
                                 implementation::VRViewSettingsControl> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
