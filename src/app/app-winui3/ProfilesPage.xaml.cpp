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
#include "ProfilesPage.xaml.h"
#include "ProfileUIData.g.cpp"
#include "ProfilesPage.g.cpp"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/ProfileSettings.h>

#include <algorithm>

#include "Globals.h"

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

static OpenKneeboardApp::ProfileUIData CreateProfileUIData(
  const ProfileSettings::Profile& profile) {
  auto ret = winrt::make<ProfileUIData>();
  ret.ID(to_hstring(profile.mID));
  ret.Name(to_hstring(profile.mName));
  ret.CanDelete(profile.mID != "default");
  return ret;
}

ProfilesPage::ProfilesPage() {
  InitializeComponent();

  const auto profileSettings = gKneeboard->GetProfileSettings();
  std::vector<ProfileSettings::Profile> profiles;
  for (const auto& [id, profile]: profileSettings.mProfiles) {
    profiles.push_back(profile);
  }
  std::ranges::sort(profiles, [](const auto& a, const auto& b) {
    if (a.mID == "default") {
      return true;
    }
    if (b.mID == "default") {
      return false;
    }
    return a.mName < b.mName;
  });

  auto uiProfiles = winrt::single_threaded_observable_vector<IInspectable>();

  for (const auto& profile: profiles) {
    uiProfiles.Append(CreateProfileUIData(profile));
  }
  List().ItemsSource(uiProfiles);
  // Using int32_t to match SelectedIndex
  for (int32_t i = 0; i < profiles.size(); ++i) {
    if (profiles.at(i).mID == profileSettings.mActiveProfile) {
      List().SelectedIndex(i);
      return;
    }
  }
}

fire_and_forget ProfilesPage::RemoveProfile(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  co_return;
}

void ProfilesPage::CreateProfile(
  const IInspectable& sender,
  const RoutedEventArgs&) noexcept {
}

hstring ProfileUIData::ID() {
  return mID;
}

void ProfileUIData::ID(hstring value) {
  mID = value;
}

hstring ProfileUIData::Name() {
  return mName;
}

void ProfileUIData::Name(hstring value) {
  mName = value;
}

bool ProfileUIData::CanDelete() {
  return mCanDelete;
}

void ProfileUIData::CanDelete(bool value) {
  mCanDelete = value;
}

}// namespace winrt::OpenKneeboardApp::implementation
