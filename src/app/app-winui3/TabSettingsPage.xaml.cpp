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
#include "TabSettingsPage.xaml.h"
#include "TabData.g.cpp"
#include "TabSettingsPage.g.cpp"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabState.h>
#include <OpenKneeboard/TabTypes.h>

#include "Globals.h"

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

TabSettingsPage::TabSettingsPage() {
  InitializeComponent();

  auto tabs = winrt::single_threaded_vector<IInspectable>();
  for (const auto& tabState: gKneeboard->GetTabs()) {
    auto winrtTab = winrt::make<TabData>();
    winrtTab.Title(to_hstring(tabState->GetRootTab()->GetTitle()));
    winrtTab.UniqueID(tabState->GetInstanceID());
    tabs.Append(winrtTab);
  }
  List().ItemsSource(tabs);

  auto tabTypes = AddTabFlyout().Items();
#define IT(label, name) \
  MenuFlyoutItem name##Item; \
  name##Item.Text(to_hstring(label)); \
  name##Item.Tag(box_value<uint64_t>(TABTYPE_IDX_##name)); \
  name##Item.Click({this, &TabSettingsPage::AddTab}); \
  tabTypes.Append(name##Item);
  OPENKNEEBOARD_TAB_TYPES
#undef IT
}

fire_and_forget TabSettingsPage::AddTab(const IInspectable& sender, const RoutedEventArgs&) {
  auto id = unbox_value<uint64_t>(sender.as<MenuFlyoutItem>().Tag());
  co_return;
}

hstring TabData::Title() {
  return mTitle;
}

void TabData::Title(hstring value) {
  mTitle = value;
}

uint64_t TabData::UniqueID() {
  return mUniqueID;
}

void TabData::UniqueID(uint64_t value) {
  mUniqueID = value;
}

}// namespace winrt::OpenKneeboardApp::implementation
