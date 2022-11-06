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
#include <OpenKneeboard/utf8.h>

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
  this->UpdateList();

  AddEventListener(gKneeboard->evProfileSettingsChangedEvent, [this]() {
    const auto settings = gKneeboard->GetProfileSettings();
    const auto sorted = settings.GetSortedProfiles();
    // Using int32_t and it's what SelectedIndex() requires
    const auto size = std::min(
      static_cast<int32_t>(sorted.size()),
      static_cast<int32_t>(List().Items().Size()));
    for (int32_t i = 0; i < size; ++i) {
      if (sorted.at(i).mID != settings.mActiveProfile) {
        continue;
      }
      List().SelectedIndex(i);
      return;
    }
  });
}

void ProfilesPage::final_release(std::unique_ptr<ProfilesPage> page) {
  page->RemoveAllEventListeners();
}

void ProfilesPage::UpdateList() {
  const auto profileSettings = gKneeboard->GetProfileSettings();
  const auto profiles = profileSettings.GetSortedProfiles();

  mUIProfiles.Clear();

  for (const auto& profile: profiles) {
    mUIProfiles.Append(CreateProfileUIData(profile));
  }
  List().ItemsSource(mUIProfiles);
  // Using int32_t to match SelectedIndex
  for (int32_t i = 0; i < profiles.size(); ++i) {
    if (profiles.at(i).mID == profileSettings.mActiveProfile) {
      List().SelectedIndex(i);
      return;
    }
  }
}

void ProfilesPage::OnList_SelectionChanged(
  const IInspectable&,
  const SelectionChangedEventArgs& args) {
  auto it = args.AddedItems().First();
  if (!it.HasCurrent()) {
    return;
  }
  const auto selectedID {to_string(it.Current().as<ProfileUIData>()->ID())};

  auto profileSettings = gKneeboard->GetProfileSettings();
  if (profileSettings.mActiveProfile == selectedID) {
    return;
  }
  profileSettings.mActiveProfile = selectedID;
  gKneeboard->SetProfileSettings(profileSettings);
}

fire_and_forget ProfilesPage::RemoveProfile(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  const auto id {to_string(unbox_value<hstring>(sender.as<Button>().Tag()))};
  auto profileSettings = gKneeboard->GetProfileSettings();
  const auto profile = profileSettings.mProfiles.at(id);
  const auto sorted = profileSettings.GetSortedProfiles();
  const auto index = std::ranges::find(sorted, profile) - sorted.begin();

  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(_(L"Remove profile?")));
  dialog.Content(box_value(to_hstring(std::format(
    _("Are you sure you want to delete the profile '{}'?"), profile.mName))));
  dialog.PrimaryButtonText(
    to_hstring(std::format(_("Delete '{}'"), profile.mName)));
  dialog.CloseButtonText(_(L"Cancel"));
  dialog.DefaultButton(ContentDialogButton::Close);

  const auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  // Actually erase the settings
  const auto parentSettings = Settings::Load("default");
  parentSettings.Save(id);

  // ... and remove from the list
  if (id == profileSettings.mActiveProfile) {
    profileSettings.mActiveProfile = "default";
  }
  profileSettings.mProfiles.erase(id);
  gKneeboard->SetProfileSettings(profileSettings);
  mUIProfiles.RemoveAt(static_cast<uint32_t>(index));
  List().SelectedIndex(0);
}

fire_and_forget ProfilesPage::CreateProfile(
  const IInspectable& sender,
  const RoutedEventArgs&) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(_(L"Create a profile")));
  dialog.PrimaryButtonText(_(L"Create"));
  dialog.IsPrimaryButtonEnabled(false);
  dialog.CloseButtonText(_(L"Cancel"));
  dialog.DefaultButton(ContentDialogButton::Primary);

  TextBlock label;
  label.Text(_(L"What do you want to call your new profile?"));

  TextBox textBox;
  textBox.TextChanged([=](const auto&, const auto&) {
    dialog.IsPrimaryButtonEnabled(!textBox.Text().empty());
  });

  StackPanel layout;
  layout.Orientation(Orientation::Vertical);
  layout.Children().Append(label);
  layout.Children().Append(textBox);
  dialog.Content(layout);

  auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  auto profileSettings = gKneeboard->GetProfileSettings();
  const auto name = to_string(textBox.Text());
  const ProfileSettings::Profile profile {
    .mID = profileSettings.MakeID(name),
    .mName = name,
  };

  profileSettings.mActiveProfile = profile.mID;
  profileSettings.mProfiles[profile.mID] = profile;
  gKneeboard->SetProfileSettings(profileSettings);

  const auto sorted = profileSettings.GetSortedProfiles();
  for (int32_t i = 0; i < sorted.size(); ++i) {
    const auto& it = sorted.at(i);
    if (it.mID != profile.mID) {
      continue;
    }
    mUIProfiles.InsertAt(i, CreateProfileUIData(profile));
    List().SelectedIndex(i);
    break;
  }
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
