// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "HelpPage.g.h"
#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.hpp>

#include <filesystem>
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
  OpenKneeboard::fire_and_forget OnExportClick(
    IInspectable,
    RoutedEventArgs) noexcept;

  OpenKneeboard::fire_and_forget OnCheckForUpdatesClick(
    IInspectable,
    RoutedEventArgs) noexcept;

  bool AgreedToPrivacyWarning() noexcept;
  bool AgreeButtonIsEnabled() noexcept;

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
