// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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
