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
#include "HelpPage.xaml.h"
#include "HelpPage.g.cpp"
// clang-format on

#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/TroubleshootingStore.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/version.h>
#include <appmodel.h>
#include <microsoft.ui.xaml.window.h>
#include <shellapi.h>
#include <shims/winrt/base.h>
#include <shobjidl.h>
#include <time.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/windows.storage.pickers.h>

#include <format>
#include <fstream>
#include <string>

#include "CheckForUpdates.h"

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

HelpPage::HelpPage() {
  InitializeComponent();

  // The one true terminal size is 80x24, fight me.
  DPrintScroll().MaxHeight(DPrintText().FontSize() * 24);

  this->PopulateVersion();
  this->PopulateEvents();
  this->PopulateDPrint();
  this->PopulateLicenses();

  AddEventListener(
    TroubleshootingStore::Get()->evGameEventReceived,
    &HelpPage::PopulateEvents,
    this);

  auto weakThis = this->get_weak();
  AddEventListener(
    TroubleshootingStore::Get()->evDPrintMessageReceived, [weakThis]() {
      [weakThis]() noexcept -> winrt::fire_and_forget {
        // always force a reschedule
        co_await winrt::resume_background();
        auto strongThis = weakThis.get();
        if (strongThis) {
          strongThis->PopulateDPrint();
        }
      }();
    });

  QuickStartLink().Click([](auto&, auto&) -> winrt::fire_and_forget {
    const auto quickStartPath = RuntimeFiles::GetInstallationDirectory()
      / RuntimeFiles::QUICK_START_PDF;

    co_await LaunchURI(to_utf8(quickStartPath));
  });
}

HelpPage::~HelpPage() {
  this->RemoveAllEventListeners();
}

void HelpPage::PopulateVersion() {
  const auto version = std::format(
    "{}.{}.{}.{}-{}{}{}{}",
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build,
    Version::CommitIDA.substr(0, 7),
    Version::HaveModifiedFiles ? "-dirty" : "",
    Version::IsGithubActionsBuild ? "-gha" : "-local",
#ifdef DEBUG
    "-debug"
#else
    ""
#endif
  );

  auto commitTime = std::chrono::time_point_cast<std::chrono::seconds>(
    std::chrono::system_clock::from_time_t(Version::CommitUnixTimestamp));

  UINT32 packageNameLen = MAX_PATH;
  wchar_t packageName[MAX_PATH];
  if (
    GetCurrentPackageFullName(&packageNameLen, packageName) == ERROR_SUCCESS) {
    packageNameLen -= 1;// null terminator
  } else {
    packageNameLen = 0;
  }

  auto details = std::format(
    "OpenKneeboard {}\n\n"
    "Copyright © 2021-2022 Frederick Emmott.\n\n"
    "With thanks to Paul 'Goldwolf' Whittingham for the logo and banner "
    "artwork.\n\n"
    "v{}\n"
    "Package: {}\n"
    "Built at: {}\n"
    "Build type: {}-{}\n"
    "Commited at: {:%Y-%m-%dT%H:%M:%SZ}\n"
    "Commit ID: {}\n",
    Version::ReleaseName,
    version,
    packageNameLen ? to_utf8(std::wstring_view(packageName, packageNameLen))
                   : "Unpackaged",
    Version::BuildTimestamp,
    BUILD_CONFIG,
#ifdef _WIN32
#ifdef _WIN64
    "Win64",
#else
    "Win32",
#endif
#endif
    commitTime,
    Version::CommitIDA);
  if (Version::HaveModifiedFiles) {
    std::string files = Version::ModifiedFiles;
    details += "\nModified files:\n" + files;
  }
  VersionText().Text(winrt::to_hstring(details));

  mVersionClipboardData = details;
}

static inline void SetClipboardText(std::string_view text) {
  Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(winrt::to_hstring(text));
  Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
}

static inline void SetClipboardText(std::wstring_view text) {
  Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(text);
  Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
}

void HelpPage::OnCopyVersionDataClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(mVersionClipboardData);
}

void HelpPage::OnCopyGameEventsClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(mGameEventsClipboardData);
}

winrt::fire_and_forget HelpPage::OnExportClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  Windows::Storage::Pickers::FileSavePicker picker;

  picker.as<IInitializeWithWindow>()->Initialize(gMainWindow);
  picker.SettingsIdentifier(L"openkneeboard/exportDebugInfo");
  picker.SuggestedStartLocation(
    Windows::Storage::Pickers::PickerLocationId::Desktop);

  auto plainTextExtensions {winrt::single_threaded_vector<winrt::hstring>()};
  plainTextExtensions.Append(L".txt");

  picker.FileTypeChoices().Insert(L"Plain Text", plainTextExtensions);
  picker.SuggestedFileName(std::format(
    L"OpenKneeboard-{}.{}.{}.{}.txt",
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build));

  auto file = co_await picker.PickSaveFileAsync();
  if (!file) {
    co_return;
  }

  auto path = file.Path();
  if (path.empty()) {
    co_return;
  }

  auto sep = "\n\n--------------------------------\n\n";

  auto now = std::chrono::time_point_cast<std::chrono::seconds>(
    std::chrono::system_clock::now());
  std::ofstream(std::filesystem::path {std::wstring_view {path}})
    << std::format(
         "Local time: {:%F %T%z}\n"
         "UTC time:   {:%F %T}",
         std::chrono::zoned_time(std::chrono::current_zone(), now),
         std::chrono::zoned_time("UTC", now))
    << sep << mVersionClipboardData << sep << mGameEventsClipboardData << sep
    << winrt::to_string(mDPrintClipboardData) << std::endl;
}

void HelpPage::OnCopyDPrintClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(mDPrintClipboardData);
}

template <class C, class T>
auto ReadableTime(const std::chrono::time_point<C, T>& time) {
  return std::chrono::zoned_time(
    std::chrono::current_zone(),
    std::chrono::time_point_cast<std::chrono::seconds>(time));
}

winrt::fire_and_forget HelpPage::PopulateEvents() noexcept {
  auto events = TroubleshootingStore::Get()->GetGameEvents();

  std::string message;

  if (events.empty()) {
    message = std::format(
      "No events as of {}", ReadableTime(std::chrono::system_clock::now()));
  }

  for (const auto& event: events) {
    message += "\n\n";

    std::string_view name(event.mName);
    const char prefix[] = "com.fredemmott.openkneeboard";
    if (name.starts_with(prefix)) {
      name.remove_prefix(sizeof(prefix));
    }
    if (name.starts_with('.')) {
      name.remove_prefix('.');
    }

    message += std::format(
      "{}:\n"
      "  Latest value:  '{}'\n"
      "  First seen:    {}\n"
      "  Last seen:     {}\n"
      "  Receive count: {}\n"
      "  Change count:  {}",
      name,
      event.mValue,
      ReadableTime(event.mFirstSeen),
      ReadableTime(event.mLastSeen),
      event.mReceiveCount,
      event.mUpdateCount);
  }

  mGameEventsClipboardData = message;

  co_await mUIThread;
  EventsText().Text(winrt::to_hstring(message));
}

winrt::fire_and_forget HelpPage::PopulateDPrint() noexcept {
  auto messages = TroubleshootingStore::Get()->GetDPrintMessages();

  std::wstring text;
  if (messages.empty()) {
    text = L"No log messages (?!)";
  }

  bool first = true;
  for (const auto& entry: messages) {
    if (first) [[unlikely]] {
      first = false;
    } else {
      text += L'\n';
    }

    auto exe = std::wstring_view(entry.mMessage.mExecutable);
    {
      auto dirSep = exe.find_last_of(L'\\');
      if (dirSep != exe.npos && dirSep + 1 < exe.size()) {
        exe.remove_prefix(dirSep + 1);
      }
    }

    text += std::format(
      L"[{:%F %T} {} ({})] {}: {}",
      ReadableTime(entry.mWhen),
      exe,
      entry.mMessage.mProcessID,
      entry.mMessage.mPrefix,
      entry.mMessage.mMessage);
  }

  mDPrintClipboardData = text;
  co_await mUIThread;
  DPrintText().Text(text);
  this->ScrollDPrintToEnd();
}

void HelpPage::OnDPrintLayoutChanged(
  const IInspectable&,
  const IInspectable&) noexcept {
  if (DPrintExpander().IsExpanded()) {
    if (!mWasDPrintExpanded) {
      this->ScrollDPrintToEnd();
    }
    mWasDPrintExpanded = true;
  } else {
    mWasDPrintExpanded = false;
  }
}

void HelpPage::ScrollDPrintToEnd() {
  DPrintScroll().UpdateLayout();
  DPrintScroll().ChangeView({}, {DPrintScroll().ScrollableHeight()}, {});
}

winrt::fire_and_forget HelpPage::OnCheckForUpdatesClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  co_await CheckForUpdates(UpdateCheckType::Manual, this->XamlRoot());
}

void HelpPage::PopulateLicenses() noexcept {
  const auto docDir = Filesystem::GetRuntimeDirectory() / "share" / "doc";

  if (!std::filesystem::exists(docDir)) {
    return;
  }

  auto children = Licenses().Children();
  children.Clear();

  auto addEntry
    = [&](const std::string& label, const std::filesystem::path& path) {
        Controls::HyperlinkButton link;
        link.Content(box_value(to_hstring(label)));
        link.Click(
          [=](const auto&, const auto&) { this->DisplayLicense(label, path); });
        children.Append(link);
      };

  addEntry("OpenKneeboard", docDir / "LICENSE.txt");
  addEntry("GNU General Public License, Version 2", docDir / "gpl-2.0.txt");

  Controls::TextBlock ackBlock;
  ackBlock.TextWrapping(TextWrapping::WrapWholeWords);
  ackBlock.Text(_(
    L"OpenKneeboard uses and includes software from the following projects:"));
  children.Append(ackBlock);

  for (const auto& entry: std::filesystem::directory_iterator(docDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto label = to_utf8(entry.path().stem());
    if (!label.starts_with("LICENSE-ThirdParty-")) {
      continue;
    }
    label.erase(0, sizeof("LICENSE-ThirdParty-") - 1 /* trailing null */);

    addEntry(label, entry.path());
  }
}

void HelpPage::DisplayLicense(
  const std::string& /* title */,
  const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    return;
  }

  ShellExecuteW(NULL, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}// namespace winrt::OpenKneeboardApp::implementation
