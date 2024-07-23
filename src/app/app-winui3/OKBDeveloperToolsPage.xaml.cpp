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
  const auto commandLine = GetOpenPluginCommandLine();
  RegSetKeyValueW(
    HKEY_CURRENT_USER,
    std::format(
      L"Software\\Classes\\{}\\shell\\open\\command", PluginHandlerName)
      .c_str(),
    /* default value */ nullptr,
    REG_SZ,
    commandLine.c_str(),
    static_cast<DWORD>(commandLine.size() * sizeof(commandLine[0])));

  // Also register an icon
  const auto icon
    = std::format(L"{},0", Filesystem::GetCurrentExecutablePath());
  RegSetKeyValueW(
    HKEY_CURRENT_USER,
    std::format(L"Software\\Classes\\{}\\DefaultIcon", PluginHandlerName)
      .c_str(),
    /* default value */ nullptr,
    REG_SZ,
    icon.c_str(),
    static_cast<DWORD>(icon.size() * sizeof(icon[0])));

  // ... and let's not just leave it saying 'OPENKNEEBOARDPLUGIN File'
  const std::wstring fileTypeName {L"OpenKneeboard Plugin"};
  RegSetKeyValueW(
    HKEY_CURRENT_USER,
    std::format(L"Software\\Classes\\{}", PluginHandlerName).c_str(),
    L"FriendlyTypeName",
    REG_SZ,
    fileTypeName.c_str(),
    static_cast<DWORD>(fileTypeName.size() * sizeof(fileTypeName[0])));

  // ... or 'Open With' -> 'OpenKneeboardApp'
  const std::wstring appName {L"OpenKneeboard - Dev"};
  RegSetKeyValueW(
    HKEY_CURRENT_USER,
    std::format(L"Software\\Classes\\{}\\shell\\open", PluginHandlerName)
      .c_str(),
    L"FriendlyAppName",
    REG_SZ,
    appName.c_str(),
    static_cast<DWORD>(appName.size() * sizeof(appName[0])));

  // Register the extension, and tie it to that handler
  RegSetKeyValueW(
    HKEY_CURRENT_USER,
    L"Software\\Classes\\.OpenKneeboardPlugin",
    /* default value */ nullptr,
    REG_SZ,
    PluginHandlerName,
    static_cast<DWORD>(
      (std::size(PluginHandlerName) - 1) * sizeof(PluginHandlerName[0])));
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
  wchar_t buf[MAX_PATH];
  constexpr auto bufByteSize = static_cast<DWORD>(sizeof(buf) * sizeof(buf[0]));
  DWORD byteCount = bufByteSize;

  if (
    RegGetValueW(
      HKEY_CURRENT_USER,
      std::format(
        L"Software\\Classes\\{}\\shell\\open\\command", PluginHandlerName)
        .c_str(),
      nullptr,
      RRF_RT_REG_SZ,
      nullptr,
      buf,
      &byteCount)
    != ERROR_SUCCESS) {
    return false;
  }

  {
    const std::wstring_view registryCommandLine {
      buf, (byteCount / sizeof(wchar_t)) - 1};
    if (registryCommandLine != GetOpenPluginCommandLine()) {
      return false;
    }
  }

  byteCount = bufByteSize;
  if (
    RegGetValueW(
      HKEY_CURRENT_USER,
      L"Software\\Classes\\.OpenKneeboardPlugin",
      nullptr,
      RRF_RT_REG_SZ,
      nullptr,
      buf,
      &byteCount)
    != ERROR_SUCCESS) {
    return false;
  }
  {
    const std::wstring_view handlerName {
      buf, (byteCount / sizeof(wchar_t)) - 1};
    if (handlerName != PluginHandlerName) {
      return false;
    }
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
