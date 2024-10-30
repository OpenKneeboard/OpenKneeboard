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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <OpenKneeboard/DeveloperToolsSettingsPage.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>

#include <OpenKneeboard/format/enum.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <wil/registry.h>

#include <format>
#include <string>

namespace OpenKneeboard {

// I'd prefer OpenKneeboard.Dev.Plugin, but MSDN recommends version at the end
static constexpr wchar_t PluginHandlerName[] {L"OpenKneeboard.Plugin.Dev"};

static auto GetOpenPluginCommandLine() {
  return std::format(
    L"\"{}\" --plugin \"%1\"", Filesystem::GetCurrentExecutablePath());
}

DeveloperToolsSettingsPage::DeveloperToolsSettingsPage(KneeboardState* kbs)
  : mKneeboard(kbs) {
}

std::filesystem::path DeveloperToolsSettingsPage::GetAppWebViewSourcePath()
  const {
  // Intentionally not using `AppSettings::GetAppWebViewSourcePath()` as the
  // settings UI should be aware of if it's explicitly set to the default or
  // just unset
  return mKneeboard->GetAppSettings().mAppWebViewSourcePath;
}
fire_and_forget DeveloperToolsSettingsPage::SetAppWebViewSourcePath(
  const std::filesystem::path v) {
  auto s = mKneeboard->GetAppSettings();
  s.mAppWebViewSourcePath = v;
  co_await mKneeboard->SetAppSettings(s);
}

std::filesystem::path
DeveloperToolsSettingsPage::GetDefaultAppWebViewSourcePath() const {
  return AppSettings::GetDefaultAppWebViewSourcePath();
}

bool DeveloperToolsSettingsPage::GetIsPluginFileTypeInHKCU() const {
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

static void RegisterFileTypeInHKCU() {
  // App registration, and handler for the 'open' action
  wil::reg::set_value_string(
    HKEY_CURRENT_USER,
    std::format(
      L"Software\\Classes\\{}\\shell\\open\\command", PluginHandlerName)
      .c_str(),
    /* default value */
    nullptr,
    GetOpenPluginCommandLine().c_str());

  // Also register an icon
  wil::reg::set_value_string(
    HKEY_CURRENT_USER,
    std::format(L"Software\\Classes\\{}\\DefaultIcon", PluginHandlerName)
      .c_str(),
    /* default value */
    nullptr,
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
    /* default value */
    nullptr,
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

fire_and_forget DeveloperToolsSettingsPage::SetIsPluginFileTypeInHKCU(
  bool enabled) {
  if (enabled) {
    RegisterFileTypeInHKCU();
  } else {
    UnregisterFileTypeInHKCU();
  }
  co_return;
}

std::string DeveloperToolsSettingsPage::GetAutoUpdateFakeCurrentVersion()
  const {
  return mKneeboard->GetAppSettings().mAutoUpdate.mTesting.mFakeCurrentVersion;
}

std::string DeveloperToolsSettingsPage::GetActualCurrentVersion() const {
  return Version::ReleaseName;
}

fire_and_forget DeveloperToolsSettingsPage::SetAutoUpdateFakeCurrentVersion(
  const std::string value) {
  auto settings = mKneeboard->GetAppSettings();
  settings.mAutoUpdate.mTesting.mFakeCurrentVersion = std::string {value};
  co_await mKneeboard->SetAppSettings(settings);
}

static inline void SetClipboardText(std::string_view text) {
  winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(winrt::to_hstring(text));
  winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(
    package);
}

fire_and_forget DeveloperToolsSettingsPage::CopyAPIEventsToClipboard() const {
  SetClipboardText(TroubleshootingStore::Get()->GetAPIEventsDebugLogAsString());
  co_return;
}

fire_and_forget DeveloperToolsSettingsPage::CopyDebugMessagesToClipboard()
  const {
  SetClipboardText(TroubleshootingStore::Get()->GetDPrintDebugLogAsString());
  co_return;
}

fire_and_forget DeveloperToolsSettingsPage::TriggerCrash(
  CrashKind kind,
  CrashLocation location) const {
  static constexpr const char TriggeredCrashMessage[]
    = "'Trigger crash' clicked on developer tools page";

  // We need the unreachable co_return to make the functions coroutines, so we
  // get task<void>'s exception handler
  std::function<task<void>()> triggerCrash;
  switch (kind) {
    case CrashKind::Fatal:
      triggerCrash = []() -> task<void> {
        fatal("{}", TriggeredCrashMessage);
        co_return;
      };
      break;
    case CrashKind::Throw:
      triggerCrash = []() -> task<void> {
        throw std::runtime_error(TriggeredCrashMessage);
        co_return;
      };
      break;
    case CrashKind::ThrowFromFireAndForget:
      triggerCrash = []() -> task<void> {
        []() -> OpenKneeboard::fire_and_forget {
          throw std::runtime_error(TriggeredCrashMessage);
          co_return;
        }();
        co_return;
      };
      break;
    case CrashKind::ThrowFromNoexcept:
      triggerCrash = []() noexcept -> task<void> {
        throw std::runtime_error(TriggeredCrashMessage);
        co_return;
      };
      break;
    case CrashKind::Terminate:
      triggerCrash = []() -> task<void> {
        std::terminate();
        co_return;
      };
      break;
    default:
      OPENKNEEBOARD_BREAK;
      // Error: task failed successfully 🤷
      fatal("Invalid CrashKind selected from dev tools page");
  }

  if (!triggerCrash) {
    OPENKNEEBOARD_BREAK;
    fatal("triggerCrash not set");
  }

  switch (location) {
    case CrashLocation::UIThread: {
      co_await triggerCrash();
      std::unreachable();
    }
    case CrashLocation::MUITask: {
      auto dqc = winrt::Microsoft::UI::Dispatching::DispatcherQueueController::
        CreateOnDedicatedThread();
      co_await wil::resume_foreground(dqc.DispatcherQueue());
      co_await triggerCrash();
      std::unreachable();
    }
    case CrashLocation::WindowsSystemTask: {
      auto dqc = winrt::Windows::System::DispatcherQueueController::
        CreateOnDedicatedThread();
      co_await wil::resume_foreground(dqc.DispatcherQueue());
      co_await triggerCrash();
      std::unreachable();
    }
    default:
      OPENKNEEBOARD_BREAK;
      fatal("Unhandled CrashLocation from devtools page: {}", location);
      co_return;
  }
}

}// namespace OpenKneeboard