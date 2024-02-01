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

using namespace winrt::Microsoft::UI::Xaml;

namespace OpenKneeboard {
class KneeboardState;
}

namespace winrt::OpenKneeboardApp::implementation {
struct VRViewSettingsControl
  : VRViewSettingsControlT<VRViewSettingsControl>,
    OpenKneeboard::WithPropertyChangedEventOnProfileChange<
      VRViewSettingsControl> {
  VRViewSettingsControl();

  winrt::guid ViewID();
  void ViewID(const winrt::guid&);

 private:
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;

  winrt::guid mViewID;

  bool mHaveSubcontrol = false;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct VRViewSettingsControl : VRViewSettingsControlT<
                                 VRViewSettingsControl,
                                 implementation::VRViewSettingsControl> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
