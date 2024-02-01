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
#include "VRSettingsPage.xaml.h"
#include "VRSettingsPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/RuntimeFiles.h>

#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/weak_wrap.h>

#include <shims/filesystem>

#include <cmath>
#include <numbers>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

static const wchar_t gOpenXRLayerSubkey[]
  = L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit";

VRSettingsPage::VRSettingsPage() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
  this->PopulateViews();
}

fire_and_forget VRSettingsPage::RestoreDefaults(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default VR settings, "
                           "removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  mKneeboard->ResetVRSettings();

  if (!mPropertyChangedEvent) {
    co_return;
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

bool VRSettingsPage::SteamVREnabled() {
  return mKneeboard->GetVRSettings().mEnableSteamVR;
}

void VRSettingsPage::SteamVREnabled(bool enabled) {
  auto config = mKneeboard->GetVRSettings();
  config.mEnableSteamVR = enabled;
  mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::OpenXREnabled() noexcept {
  DWORD data {};
  DWORD dataSize = sizeof(data);
  const auto jsonPath
    = std::filesystem::canonical(
        RuntimeFiles::GetInstallationDirectory() / RuntimeFiles::OPENXR_JSON)
        .wstring();
  const auto result = RegGetValueW(
    HKEY_LOCAL_MACHINE,
    gOpenXRLayerSubkey,
    jsonPath.c_str(),
    RRF_RT_DWORD,
    nullptr,
    &data,
    &dataSize);
  if (result != ERROR_SUCCESS) {
    return false;
  }
  const auto disabled = static_cast<bool>(data);
  return !disabled;
}

fire_and_forget VRSettingsPage::RemoveKneeboard(
  muxc::TabView tabView,
  muxc::TabViewTabCloseRequestedEventArgs args) noexcept {
  const auto guid = unbox_value<winrt::guid>(args.Tab().Tag());

  {
    const auto views = mKneeboard->GetViewsSettings().mViews;
    const auto it
      = std::ranges::find(views, guid, [](const auto& it) { return it.mGuid; });

    const auto message = std::format(
      _("Do you want to completely remove \"{}\" and delete all its' "
        "settings?"),
      it->mName);

    ContentDialog dialog;
    dialog.XamlRoot(XamlRoot());
    dialog.Title(winrt::box_value(winrt::to_hstring(_("Delete view?"))));
    dialog.DefaultButton(ContentDialogButton::Primary);
    dialog.PrimaryButtonText(winrt::to_hstring(_("Delete")));
    dialog.CloseButtonText(winrt::to_hstring(_("Cancel")));
    dialog.Content(box_value(to_hstring(message)));

    if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
      co_return;
    }
  }

  // While it was modal and nothing else 'should' have changed things in the
  // mean time, re-fetch just in case
  {
    auto settings = mKneeboard->GetViewsSettings();
    auto it = std::ranges::find(
      settings.mViews, guid, [](const auto& it) { return it.mGuid; });
    settings.mViews.erase(it);
    mKneeboard->SetViewsSettings(settings);
  }

  {
    auto items = tabView.TabItems();
    auto it = std::ranges::find(items, args.Tab());
    items.RemoveAt(static_cast<uint32_t>(it - items.begin()));
  }
}

void VRSettingsPage::PopulateViews() noexcept {
  bool first = true;
  for (const auto& view: mKneeboard->GetViewsSettings().mViews) {
    TabViewItem tab;
    tab.Tag(winrt::box_value(view.mGuid));
    tab.Header(winrt::box_value(winrt::to_hstring(view.mName)));
    tab.IsClosable(!first);

    VRViewSettingsControl viewSettings;
    viewSettings.ViewID(view.mGuid);
    tab.Content(viewSettings);

    TabView().TabItems().Append(tab);

    first = false;
  }
}

fire_and_forget VRSettingsPage::OpenXREnabled(bool enabled) noexcept {
  if (enabled == OpenXREnabled()) {
    co_return;
  }

  const auto newValue = enabled ? OpenXRMode::AllUsers : OpenXRMode::Disabled;
  const auto oldValue = enabled ? OpenXRMode::Disabled : OpenXRMode::AllUsers;
  co_await SetOpenXRModeWithHelperProcess(newValue, {oldValue});
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"OpenXREnabled"));
}

}// namespace winrt::OpenKneeboardApp::implementation
