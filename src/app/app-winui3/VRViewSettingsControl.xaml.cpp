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

#include <OpenKneeboard/utf8.h>

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

IInspectable VRViewSettingsControl::SelectedKind() {
  const auto views = mKneeboard->GetViewsSettings().mViews;
  const auto it = std::ranges::find(
    views, mViewID, [](const auto& it) { return it.mGuid; });
  using Type = ViewVRConfig::Type;
  const auto type = it->mVR.GetType();

  for (const auto& item: Kind().Items()) {
    if (type == Type::Independent && item.try_as<IndependentVRViewUIKind>()) {
      return item;
    }

    if (type == Type::HorizontalMirror) {
      const auto mirror = item.try_as<HorizontalMirrorVRViewUIKind>();
      if (mirror && mirror.MirrorOf() == it->mVR.GetMirrorOfGUID()) {
        return item;
      }
    }
  }

  return {nullptr};
}

void VRViewSettingsControl::SelectedKind(const IInspectable& item) {
  auto settings = mKneeboard->GetViewsSettings();
  auto it = std::ranges::find(
    settings.mViews, mViewID, [](const auto& it) { return it.mGuid; });

  using Type = ViewVRConfig::Type;
  const auto type = it->mVR.GetType();

  if (item.try_as<IndependentVRViewUIKind>()) {
    if (type == Type::Independent) {
      return;
    }
    IndependentViewVRConfig config {};
    if (type == Type::HorizontalMirror) {
      auto other = std::ranges::find(
        settings.mViews, it->mVR.GetMirrorOfGUID(), [](const auto& it) {
          return it.mGuid;
        });
      if (
        (other != settings.mViews.end())
        && other->mVR.GetType() == Type::Independent) {
        config = other->mVR.GetIndependentConfig();
        config.mPose = config.mPose.GetHorizontalMirror();
      }
    }
    it->mVR.SetIndependentConfig(config);
  } else if (const auto mirror = item.try_as<HorizontalMirrorVRViewUIKind>()) {
    it->mVR.SetHorizontalMirrorOf(mirror.MirrorOf());
  } else {
    OPENKNEEBOARD_BREAK;
  }

  mKneeboard->SetViewsSettings(settings);

  this->PopulateSubcontrol(it->mVR);
}

void VRViewSettingsControl::PopulateKind(const ViewVRConfig& view) {
  auto items = Kind().Items();
  items.Clear();

  using Type = ViewVRConfig::Type;
  const auto type = view.GetType();

  items.Append(IndependentVRViewUIKind {});
  if (type == Type::Independent) {
    Kind().SelectedIndex(0);
  }

  for (const auto& other: mKneeboard->GetViewsSettings().mViews) {
    if (other.mGuid != mViewID && other.mVR.GetType() == Type::Independent) {
      HorizontalMirrorVRViewUIKind item;
      item.Label(
        to_hstring(std::format(_("Horizontal mirror of '{}'"), other.mName)));
      item.MirrorOf(other.mGuid);
      items.Append(item);
      if (
        type == Type::HorizontalMirror
        && view.GetMirrorOfGUID() == other.mGuid) {
        Kind().SelectedIndex(items.Size() - 1);
      }
    }
  }
}

bool VRViewSettingsControl::IsEnabledInVR() {
  const auto views = mKneeboard->GetViewsSettings().mViews;
  auto it = std::ranges::find(
    views, mViewID, [](const auto& it) { return it.mGuid; });
  return it->mVR.mEnabled;
}

void VRViewSettingsControl::IsEnabledInVR(bool value) {
  auto settings = mKneeboard->GetViewsSettings();
  auto it = std::ranges::find(
    settings.mViews, mViewID, [](const auto& it) { return it.mGuid; });
  it->mVR.mEnabled = value;
  mKneeboard->SetViewsSettings(settings);
  if (mSubControl) {
    mSubControl.IsEnabled(value);
  }
}

void VRViewSettingsControl::ViewID(const winrt::guid& guid) {
  mViewID = guid;

  const auto views = mKneeboard->GetViewsSettings().mViews;
  auto it
    = std::ranges::find(views, guid, [](const auto& it) { return it.mGuid; });
  if (it == views.end()) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  this->PopulateKind(it->mVR);
  this->PopulateSubcontrol(it->mVR);
}

void VRViewSettingsControl::PopulateSubcontrol(const ViewVRConfig& vr) {
  if (mSubControl) {
    Container().Children().RemoveAtEnd();
    mSubControl = {nullptr};
  }

  using Type = ViewVRConfig::Type;
  switch (vr.GetType()) {
    case Type::Independent: {
      IndependentVRViewSettingsControl settings;
      settings.ViewID(mViewID);
      settings.IsEnabled(vr.mEnabled);
      Container().Children().Append(settings);

      mSubControl = settings;
      return;
    }
  }
}

}// namespace winrt::OpenKneeboardApp::implementation
