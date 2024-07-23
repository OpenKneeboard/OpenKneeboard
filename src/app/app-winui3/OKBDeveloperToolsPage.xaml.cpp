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
#include "OKBDeveloperToolsPage.xaml.h"
#include "OKBDeveloperToolsPage.g.cpp"
// clang-format on

using namespace ::OpenKneeboard;

#include "HelpPage.xaml.h"

#include <OpenKneeboard/Filesystem.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <wil/registry.h>

#include <mutex>

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

static inline void SetClipboardText(std::string_view text) {
  Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(winrt::to_hstring(text));
  Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
}

OKBDeveloperToolsPage::OKBDeveloperToolsPage() {
  InitializeComponent();
}

OKBDeveloperToolsPage::~OKBDeveloperToolsPage() {
}

static auto GetOpenPluginCommandLine() {
  return std::format(
    L"\"{}\" --plugin \"%1\"", Filesystem::GetCurrentExecutablePath());
}

// I'd prefer OpenKneeboard.Dev.Plugin, but MSDN recommends version at the end
static constexpr wchar_t PluginHandlerName[] {L"OpenKneeboard.Plugin.Dev"};

static void RegisterFileTypeInHKCU() {
  // App registration, and handler for the 'open' action
  wil::reg::set_value_string(
    HKEY_CURRENT_USER,
    std::format(
      L"Software\\Classes\\{}\\shell\\open\\command", PluginHandlerName)
      .c_str(),
    /* default value */ nullptr,
    GetOpenPluginCommandLine().c_str());

  // Also register an icon
  wil::reg::set_value_string(
    HKEY_CURRENT_USER,
    std::format(L"Software\\Classes\\{}\\DefaultIcon", PluginHandlerName)
      .c_str(),
    /* default value */ nullptr,
    // Reference the first icon from the exe resources
    std::format(L"{},0", Filesystem::GetCurrentExecutablePath()).c_str());

  // ... and let's not just leave it saying 'OPENKNEEBOARDPLUGIN File'
  wil::reg::set_value_string(
    HKEY_CURRENT_USER,
    std::format(L"Software\\Classes\\{}", PluginHandlerName).c_str(),
    L"FriendlyTypeName",
    L"OpenKneeboard Plugin");

  // ... or 'Open With' -> 'OpenKneeboardApp'
  wil::reg::set_value_string(
    HKEY_CURRENT_USER,
    std::format(L"Software\\Classes\\{}\\shell\\open", PluginHandlerName)
      .c_str(),
    L"FriendlyAppName",
    L"OpenKneeboard - Dev");

  // Register the extension, and tie it to that handler
  wil::reg::set_value_string(
    HKEY_CURRENT_USER,
    L"Software\\Classes\\.OpenKneeboardPlugin",
    /* default value */ nullptr,
    PluginHandlerName);
}

static void UnregisterFileTypeInHKCU() {
  // File type association
  RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.OpenKneeboardPlugin");
  // The app, and the 'Open' actions it supports
  RegDeleteTreeW(
    HKEY_CURRENT_USER,
    std::format(L"Software\\Classes\\{}", PluginHandlerName).c_str());
}

bool OKBDeveloperToolsPage::PluginFileTypeInHKCU() const noexcept {
  const auto commandLine = wil::reg::try_get_value_string(
    HKEY_CURRENT_USER,
    std::format(
      L"Software\\Classes\\{}\\shell\\open\\command", PluginHandlerName)
      .c_str(),
    nullptr);
  if (commandLine != GetOpenPluginCommandLine()) {
    return false;
  }

  const auto handlerName = wil::reg::try_get_value_string(
    HKEY_CURRENT_USER, L"Software\\Classes\\.OpenKneeboardPlugin", nullptr);
  if (handlerName != PluginHandlerName) {
    return false;
  }

  return true;
}

void OKBDeveloperToolsPage::PluginFileTypeInHKCU(bool enabled) noexcept {
  if (enabled) {
    RegisterFileTypeInHKCU();
  } else {
    UnregisterFileTypeInHKCU();
  }
}

void OKBDeveloperToolsPage::OnCopyGameEventsClick(
  const IInspectable&,
  const Microsoft::UI::Xaml::RoutedEventArgs&) noexcept {
  SetClipboardText(HelpPage::GetGameEventsAsString());
}

void OKBDeveloperToolsPage::OnCopyDebugMessagesClick(
  const IInspectable&,
  const Microsoft::UI::Xaml::RoutedEventArgs&) noexcept {
  SetClipboardText(winrt::to_string(HelpPage::GetDPrintMessagesAsWString()));
}

}// namespace winrt::OpenKneeboardApp::implementation
