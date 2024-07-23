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
#include "SettingsPage.g.h"
#include "SettingsSubpageData.g.h"
// clang-format on

#include <string>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

struct SettingsPage : SettingsPageT<SettingsPage> {
  SettingsPage();
  void OnItemClick(const IInspectable&, const ItemClickEventArgs&) noexcept;
};

struct SettingsSubpageData : SettingsSubpageDataT<SettingsSubpageData> {
  SettingsSubpageData() = default;

  hstring Glyph();
  void Glyph(hstring const& value);

  hstring Title();
  void Title(hstring const& value);

  hstring Description();
  void Description(hstring const& value);

 private:
  hstring mGlyph;
  hstring mTitle;
  hstring mDescription;
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct SettingsPage
  : SettingsPageT<SettingsPage, implementation::SettingsPage> {};
struct SettingsSubpageData : SettingsSubpageDataT<
                               SettingsSubpageData,
                               implementation::SettingsSubpageData> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
