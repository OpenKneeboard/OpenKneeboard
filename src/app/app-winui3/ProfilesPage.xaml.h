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

  fire_and_forget CreateProfile(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;
  fire_and_forget RemoveProfile(const IInspectable&, const RoutedEventArgs&);

  fire_and_forget OnList_SelectionChanged(
    const IInspectable&,
    const SelectionChangedEventArgs&);

 private:
  void UpdateList();
  Windows::Foundation::Collections::IObservableVector<IInspectable>
    mUIProfiles {single_threaded_observable_vector<IInspectable>()};
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;
};

struct ProfileUIData : ProfileUIDataT<ProfileUIData> {
  ProfileUIData() = default;

  hstring ID();
  void ID(hstring);

  hstring Name();
  void Name(hstring);

  bool CanDelete();
  void CanDelete(bool);

 private:
  hstring mID;
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
