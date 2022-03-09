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
#include "SettingsPage.xaml.h"
#include "SettingsPage.g.cpp"
#include "SettingsSubpageData.g.cpp"
// clang-format on

namespace winrt::OpenKneeboardApp::implementation {

SettingsPage::SettingsPage() {
  InitializeComponent();
}

void SettingsPage::OnItemClick(const IInspectable&, const ItemClickEventArgs& args) {
  auto item = args.ClickedItem();
  if (!item) {
    return;
  }
  const auto subpage = item.as<OpenKneeboardApp::SettingsSubpageData>().ID();
  switch (subpage) {
    case SettingsSubpageID::Tabs:
      Frame().Navigate(xaml_typename<TabSettingsPage>());
      return;
    case SettingsSubpageID::Games:
      Frame().Navigate(xaml_typename<GameSettingsPage>());
      return;
    case SettingsSubpageID::Input:
      Frame().Navigate(xaml_typename<InputSettingsPage>());
      return;
    case SettingsSubpageID::NonVRConfig:
      Frame().Navigate(xaml_typename<NonVRSettingsPage>());
      return;
  }

}

hstring SettingsSubpageData::Title() {
  return mTitle;
}

void SettingsSubpageData::Title(hstring const& value) {
  mTitle = value;
}

hstring SettingsSubpageData::Glyph() {
  return mGlyph;
}

void SettingsSubpageData::Glyph(hstring const& value) {
  mGlyph = value;
}

hstring SettingsSubpageData::Description() {
  return mDescription;
}

void SettingsSubpageData::Description(hstring const& value) {
  mDescription = value;
}

SettingsSubpageID SettingsSubpageData::ID() {
  return mID;
}

void SettingsSubpageData::ID(SettingsSubpageID value) {
  mID = value;
}

}// namespace winrt::OpenKneeboardApp::implementation
