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
// clang-format on

#include "HelpPage.g.h"
#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.hpp>

#include <shims/filesystem>

#include <string>

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct HelpPage : HelpPageT<HelpPage>,
                  private OpenKneeboard::EventReceiver,
                  public OpenKneeboard::WithPropertyChangedEvent {
  HelpPage();
  ~HelpPage();

  void OnCopyVersionDataClick(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;
  void OnAgreeClick(const IInspectable&, const RoutedEventArgs&) noexcept;
  winrt::fire_and_forget OnExportClick(IInspectable, RoutedEventArgs) noexcept;

  winrt::fire_and_forget OnCheckForUpdatesClick(
    IInspectable,
    RoutedEventArgs) noexcept;

  bool AgreedToPrivacyWarning() noexcept;
  bool AgreeButtonIsEnabled() noexcept;

  static std::string GetAPIEventsAsString() noexcept;
  static std::string GetDPrintMessagesAsString() noexcept;

 private:
  winrt::apartment_context mUIThread;
  std::string mVersionClipboardData;

  void PopulateVersion();
  void PopulateLicenses() noexcept;

  static std::string GetUpdateLog() noexcept;
  static std::string GetOpenXRInfo() noexcept;
  static std::string GetActiveConsumers() noexcept;

  void DisplayLicense(const std::string& header, const std::filesystem::path&);

  static bool mAgreedToPrivacyWarning;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct HelpPage : HelpPageT<HelpPage, implementation::HelpPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
