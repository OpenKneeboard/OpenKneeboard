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
#include "VRViewSettingsControl.xaml.h"
#include "VRViewSettingsControl.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/ViewsConfig.h>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

VRViewSettingsControl::VRViewSettingsControl() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
}

winrt::guid VRViewSettingsControl::ViewID() {
  return mViewID;
}

void VRViewSettingsControl::ViewID(const winrt::guid& guid) {
  mViewID = guid;

  if (mHaveSubcontrol) {
    Container().Children().RemoveAtEnd();
    mHaveSubcontrol = false;
  }

  const auto views = mKneeboard->GetViewsSettings().mViews;
  auto it
    = std::ranges::find(views, guid, [](const auto& it) { return it.mGuid; });
  if (it == views.end()) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  using Type = ViewVRConfig::Type;
  switch (it->mVR.GetType()) {
    case Type::Independent: {
      IndependentVRViewSettingsControl settings;
      settings.ViewID(guid);
      Container().Children().Append(settings);
      mHaveSubcontrol = true;
      return;
    }
  }
}

}// namespace winrt::OpenKneeboardApp::implementation
