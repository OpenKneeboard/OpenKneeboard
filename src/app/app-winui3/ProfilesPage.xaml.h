// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
#include "ProfileUIData.g.h"
#include "ProfilesPage.g.h"
// clang-format on

#include <OpenKneeboard/Events.hpp>

namespace OpenKneeboard {
class KneeboardState;
}

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {
struct ProfilesPage : ProfilesPageT<ProfilesPage>,
                      private OpenKneeboard::EventReceiver {
  ProfilesPage();

  static void final_release(std::unique_ptr<ProfilesPage>);

  OpenKneeboard::fire_and_forget CreateProfile(
    Windows::Foundation::IInspectable,
    RoutedEventArgs) noexcept;
  OpenKneeboard::fire_and_forget RemoveProfile(
    Windows::Foundation::IInspectable,
    RoutedEventArgs);

  OpenKneeboard::fire_and_forget OnList_SelectionChanged(
    Windows::Foundation::IInspectable,
    SelectionChangedEventArgs);

 private:
  void UpdateList();
  Windows::Foundation::Collections::IObservableVector<IInspectable>
    mUIProfiles {single_threaded_observable_vector<IInspectable>()};
  OpenKneeboard::audited_ptr<OpenKneeboard::KneeboardState> mKneeboard;
};

struct ProfileUIData : ProfileUIDataT<ProfileUIData> {
  ProfileUIData() = default;

  winrt::guid ID();
  void ID(winrt::guid);

  hstring Name();
  void Name(hstring);

  bool CanDelete();
  void CanDelete(bool);

 private:
  winrt::guid mGuid;
  hstring mName;
  bool mCanDelete {true};
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct ProfilesPage
  : ProfilesPageT<ProfilesPage, implementation::ProfilesPage> {};

struct ProfileUIData
  : ProfileUIDataT<ProfileUIData, implementation::ProfileUIData> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
