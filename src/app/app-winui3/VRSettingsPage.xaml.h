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
#include "VRSettingsPage.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ViewsConfig.hpp>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

namespace OpenKneeboard {
class KneeboardState;
}

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {
struct VRSettingsPage
  : VRSettingsPageT<VRSettingsPage>,
    WithPropertyChangedEventOnProfileChange<VRSettingsPage> {
  VRSettingsPage();
  ~VRSettingsPage();

  bool SteamVREnabled();
  OpenKneeboard::fire_and_forget SteamVREnabled(bool);

  bool OpenXR64Enabled() noexcept;
  OpenKneeboard::fire_and_forget OpenXR64Enabled(bool) noexcept;

  bool OpenXR32Enabled() noexcept;
  OpenKneeboard::fire_and_forget OpenXR32Enabled(bool) noexcept;

  OpenKneeboard::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

  OpenKneeboard::fire_and_forget AddView(muxc::TabView, IInspectable) noexcept;

  OpenKneeboard::fire_and_forget RemoveView(
    muxc::TabView,
    muxc::TabViewTabCloseRequestedEventArgs) noexcept;

 private:
  std::shared_ptr<KneeboardState> mKneeboard;
  void PopulateViews() noexcept;

  void AppendViewTab(const ViewConfig& view) noexcept;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct VRSettingsPage
  : VRSettingsPageT<VRSettingsPage, implementation::VRSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
