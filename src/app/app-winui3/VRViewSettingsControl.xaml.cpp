// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "VRViewSettingsControl.xaml.h"
#include "VRViewSettingsControl.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/TabsList.hpp>
#include <OpenKneeboard/ViewsSettings.hpp>

#include <OpenKneeboard/utf8.hpp>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

VRViewSettingsControl::VRViewSettingsControl() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
  AddEventListener(mKneeboard->evSettingsChangedEvent, [this]() {
    this->EmitPropertyChangedEvent(L"TooManyViewsVisibility");
  });
}

VRViewSettingsControl::~VRViewSettingsControl() {
  this->RemoveAllEventListeners();
}

winrt::guid VRViewSettingsControl::ViewID() {
  return mViewID;
}

IInspectable VRViewSettingsControl::SelectedKind() {
  const auto views = mKneeboard->GetViewsSettings().mViews;
  const auto it = std::ranges::find(views, mViewID, &ViewSettings::mGuid);
  using Type = ViewVRSettings::Type;
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

OpenKneeboard::fire_and_forget VRViewSettingsControl::SelectedKind(
  IInspectable item) {
  auto settings = mKneeboard->GetViewsSettings();
  auto it = std::ranges::find(settings.mViews, mViewID, &ViewSettings::mGuid);

  using Type = ViewVRSettings::Type;
  const auto type = it->mVR.GetType();

  if (item.try_as<IndependentVRViewUIKind>()) {
    if (type == Type::Independent) {
      co_return;
    }
    IndependentViewVRSettings config {};
    if (type == Type::HorizontalMirror) {
      auto other = std::ranges::find(
        settings.mViews, it->mVR.GetMirrorOfGUID(), &ViewSettings::mGuid);
      if (
        (other != settings.mViews.end())
        && other->mVR.GetType() == Type::Independent) {
        config = other->mVR.GetIndependentSettings();
        config.mPose = config.mPose.GetHorizontalMirror();
      }
    }
    it->mVR.SetIndependentSettings(config);
  } else if (const auto mirror = item.try_as<HorizontalMirrorVRViewUIKind>()) {
    it->mVR.SetHorizontalMirrorOf(mirror.MirrorOf());
  } else {
    OPENKNEEBOARD_BREAK;
  }

  co_await mKneeboard->SetViewsSettings(settings);

  this->PopulateSubcontrol(it->mVR);
}

void VRViewSettingsControl::PopulateKind(const ViewVRSettings& view) {
  auto items = Kind().Items();
  items.Clear();

  using Type = ViewVRSettings::Type;
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

void VRViewSettingsControl::PopulateDefaultTab() {
  auto items = DefaultTab().Items();
  items.Clear();

  items.Append(UIDataItem {_(L"Automatic")});

  for (const auto& tab: mKneeboard->GetTabsList()->GetTabs()) {
    UIDataItem item;
    item.Label(to_hstring(tab->GetTitle()));
    item.Tag(box_value(tab->GetPersistentID()));
    items.Append(item);
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"SelectedDefaultTab"));
}

bool VRViewSettingsControl::IsEnabledInVR() {
  const auto views = mKneeboard->GetViewsSettings().mViews;
  auto it = std::ranges::find(views, mViewID, &ViewSettings::mGuid);
  return it->mVR.mEnabled;
}

OpenKneeboard::fire_and_forget VRViewSettingsControl::IsEnabledInVR(
  bool value) {
  auto settings = mKneeboard->GetViewsSettings();
  auto it = std::ranges::find(settings.mViews, mViewID, &ViewSettings::mGuid);
  if (it->mVR.mEnabled == value) {
    co_return;
  }
  it->mVR.mEnabled = value;
  co_await mKneeboard->SetViewsSettings(settings);
  if (mSubControl) {
    mSubControl.IsEnabled(value);
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"IsEnabledInVR"));
}

void VRViewSettingsControl::ViewID(const winrt::guid& guid) {
  mViewID = guid;

  const auto views = mKneeboard->GetViewsSettings().mViews;
  auto it = std::ranges::find(views, guid, &ViewSettings::mGuid);
  if (it == views.end()) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  this->PopulateDefaultTab();
  this->PopulateKind(it->mVR);
  this->PopulateSubcontrol(it->mVR);
}

void VRViewSettingsControl::PopulateSubcontrol(const ViewVRSettings& vr) {
  if (mSubControl) {
    Container().Children().RemoveAtEnd();
    mSubControl = {nullptr};
  }

  using Type = ViewVRSettings::Type;
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

IInspectable VRViewSettingsControl::SelectedDefaultTab() {
  const auto views = mKneeboard->GetViewsSettings().mViews;
  const auto it = std::ranges::find(views, mViewID, &ViewSettings::mGuid);

  const auto items = DefaultTab().Items();
  const auto tabID = it->mDefaultTabID;

  for (uint32_t i = 0; i < items.Size(); ++i) {
    const auto item = items.GetAt(i);
    if (tabID == unbox_value_or<guid>(item.as<UIDataItem>().Tag(), {})) {
      return item;
    }
  }

  return items.GetAt(0);
}

Visibility VRViewSettingsControl::TooManyViewsVisibility() {
  const auto settings = mKneeboard->GetViewsSettings();
  const auto it
    = std::ranges::find(settings.mViews, mViewID, &ViewSettings::mGuid);
  if (it - settings.mViews.begin() >= MaxViewCount) {
    return Visibility::Visible;
  }
  return Visibility::Collapsed;
}

OpenKneeboard::fire_and_forget VRViewSettingsControl::SelectedDefaultTab(
  IInspectable item) {
  const auto guid
    = unbox_value_or<winrt::guid>(item.as<UIDataItem>().Tag(), {});

  {
    auto settings = mKneeboard->GetViewsSettings();
    auto it = std::ranges::find(settings.mViews, mViewID, &ViewSettings::mGuid);
    it->mDefaultTabID = guid;
    co_await mKneeboard->SetViewsSettings(settings);
  }

  const auto viewStates = mKneeboard->GetAllViewsInFixedOrder();
  const auto stateIt
    = std::ranges::find(viewStates, mViewID, &KneeboardView::GetPersistentGUID);
  if (stateIt == viewStates.end()) {
    co_return;
  }

  const auto tabs = mKneeboard->GetTabsList()->GetTabs();
  const auto tabIt = std::ranges::find(tabs, guid, &ITab::GetPersistentID);
  if (tabIt == tabs.end()) {
    co_return;
  }

  (*stateIt)->SetCurrentTabByRuntimeID((*tabIt)->GetRuntimeID());
}

}// namespace winrt::OpenKneeboardApp::implementation
