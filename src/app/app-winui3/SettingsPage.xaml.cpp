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

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/version.hpp>

namespace winrt::OpenKneeboardApp::implementation {

using namespace OpenKneeboard;

SettingsPage::SettingsPage() {
  InitializeComponent();

  DWORD showDeveloperTools {Version::IsGithubActionsBuild ? 0 : 1};
  DWORD sizeOfDword {sizeof(DWORD)};
  RegGetValueW(
    HKEY_CURRENT_USER,
    Config::RegistrySubKey,
    L"ShowOKBDeveloperTools",
    RRF_RT_DWORD,
    nullptr,
    &showDeveloperTools,
    &sizeOfDword);
  if (!showDeveloperTools) {
    auto items = Grid().Items();
    uint32_t index {};
    if (items.IndexOf(OKBDeveloperToolsItem(), index)) {
      items.RemoveAt(index);
    }
  }
}

void SettingsPage::OnItemClick(
  const IInspectable&,
  const ItemClickEventArgs& args) noexcept {
  auto item = args.ClickedItem();
  if (!item) {
    return;
  }

#define IT(X) \
  if (item == X##Item()) { \
    Frame().Navigate(xaml_typename<X##SettingsPage>()); \
    return; \
  }
  IT(VR)
  IT(NonVR)
  IT(Games)
  IT(Tabs)
  IT(Input)
  IT(Advanced)
#undef IT

  if (item == OKBDeveloperToolsItem()) {
    Frame().Navigate(xaml_typename<OKBDeveloperToolsPage>());
    return;
  }

  OPENKNEEBOARD_BREAK;
}// namespace winrt::OpenKneeboardApp::implementation

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

}// namespace winrt::OpenKneeboardApp::implementation
