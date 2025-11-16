// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "ProfilesPage.xaml.h"
#include "ProfileUIData.g.cpp"
#include "ProfilesPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/ProfileSettings.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <algorithm>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

enum class ProfileKind { DefaultProfile, AdditionalProfile };
using Profile = ProfileSettings::Profile;

static OpenKneeboardApp::ProfileUIData CreateProfileUIData(
  const Profile& profile,
  const ProfileKind kind) {
  auto ret = winrt::make<ProfileUIData>();
  ret.ID(profile.mGuid);
  ret.Name(to_hstring(profile.mName));
  ret.CanDelete(kind == ProfileKind::AdditionalProfile);
  return ret;
}

ProfilesPage::ProfilesPage() {
  InitializeComponent();
  mKneeboard = gKneeboard.lock();
  this->UpdateList();

  AddEventListener(mKneeboard->evProfileSettingsChangedEvent, [this]() {
    const auto settings = mKneeboard->GetProfileSettings();
    const auto sorted = settings.GetSortedProfiles();
    // Using int32_t and it's what SelectedIndex() requires
    const auto size = std::min(
      static_cast<int32_t>(sorted.size()),
      static_cast<int32_t>(List().Items().Size()));
    for (int32_t i = 0; i < size; ++i) {
      if (sorted.at(i).mGuid != settings.mActiveProfile) {
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
  const auto profileSettings = mKneeboard->GetProfileSettings();
  const auto profiles = profileSettings.GetSortedProfiles();

  mUIProfiles.Clear();

  for (const auto& profile: profiles) {
    mUIProfiles.Append(CreateProfileUIData(
      profile,
      profile.mGuid == profileSettings.mDefaultProfile
        ? ProfileKind::DefaultProfile
        : ProfileKind::AdditionalProfile));
  }
  List().ItemsSource(mUIProfiles);
  // Using int32_t to match SelectedIndex
  for (int32_t i = 0; i < profiles.size(); ++i) {
    if (profiles.at(i).mGuid == profileSettings.mActiveProfile) {
      List().SelectedIndex(i);
      return;
    }
  }
}

OpenKneeboard::fire_and_forget ProfilesPage::OnList_SelectionChanged(
  IInspectable,
  SelectionChangedEventArgs args) {
  auto it = args.AddedItems().First();
  if (!it.HasCurrent()) {
    co_return;
  }
  const auto selectedID {it.Current().as<ProfileUIData>()->ID()};

  auto profileSettings = mKneeboard->GetProfileSettings();
  if (profileSettings.mActiveProfile == selectedID) {
    co_return;
  }
  profileSettings.mActiveProfile = selectedID;
  co_await mKneeboard->SetProfileSettings(profileSettings);
}

OpenKneeboard::fire_and_forget ProfilesPage::RemoveProfile(
  IInspectable sender,
  RoutedEventArgs) {
  const auto id {unbox_value<winrt::guid>(sender.as<Button>().Tag())};
  auto profileSettings = mKneeboard->GetProfileSettings();
  const auto profile
    = *std::ranges::find(profileSettings.mProfiles, id, &Profile::mGuid);
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

  if (id == profileSettings.mActiveProfile) {
    profileSettings.mActiveProfile = profileSettings.mDefaultProfile;
  }

  // Actually erase the settings
  const auto parentSettings = Settings::Load(
    profileSettings.mDefaultProfile, profileSettings.mDefaultProfile);
  parentSettings.Save(
    profileSettings.mDefaultProfile, profileSettings.mActiveProfile);
  // ... and remove from the list
  profileSettings.mProfiles.erase(
    std::ranges::find(profileSettings.mProfiles, id, &Profile::mGuid));

  co_await mKneeboard->SetProfileSettings(profileSettings);
  mUIProfiles.RemoveAt(static_cast<uint32_t>(index));
  List().SelectedIndex(0);
}

OpenKneeboard::fire_and_forget ProfilesPage::CreateProfile(
  IInspectable sender,
  RoutedEventArgs) noexcept {
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

  auto profileSettings = mKneeboard->GetProfileSettings();
  const auto name = to_string(textBox.Text());
  const Profile profile {
    .mName = name,
  };

  profileSettings.mActiveProfile = profile.mGuid;
  profileSettings.mProfiles.push_back(profile);
  co_await mKneeboard->SetProfileSettings(profileSettings);

  const auto sorted = profileSettings.GetSortedProfiles();
  for (int32_t i = 0; i < sorted.size(); ++i) {
    const auto& it = sorted.at(i);
    if (it.mGuid != profile.mGuid) {
      continue;
    }
    mUIProfiles.InsertAt(
      i,
      CreateProfileUIData(
        profile,
        profile.mGuid == profileSettings.mDefaultProfile
          ? ProfileKind::DefaultProfile
          : ProfileKind::AdditionalProfile));
    List().SelectedIndex(i);
    break;
  }
}

winrt::guid ProfileUIData::ID() {
  return mGuid;
}

void ProfileUIData::ID(winrt::guid value) {
  mGuid = value;
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
