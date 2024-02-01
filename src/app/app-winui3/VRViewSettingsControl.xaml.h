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
#include "VRViewSettingsControl.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/ViewsConfig.h>

namespace OpenKneeboard {
class KneeboardState;
}

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {
struct VRViewSettingsControl
  : VRViewSettingsControlT<VRViewSettingsControl>,
    OpenKneeboard::WithPropertyChangedEventOnProfileChange<
      VRViewSettingsControl> {
  VRViewSettingsControl();

  winrt::guid ViewID();
  void ViewID(const winrt::guid&);

  bool IsEnabledInVR();
  void IsEnabledInVR(bool);

  IInspectable SelectedKind();
  void SelectedKind(const IInspectable&);

 private:
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;

  winrt::guid mViewID;

  void PopulateKind(const ViewVRConfig&);
  void PopulateSubcontrol(const ViewVRConfig&);

  Control mSubControl {nullptr};
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct VRViewSettingsControl : VRViewSettingsControlT<
                                 VRViewSettingsControl,
                                 implementation::VRViewSettingsControl> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
