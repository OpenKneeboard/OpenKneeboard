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
#include "AboutPage.xaml.h"
#include "AboutPage.g.cpp"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TroubleshootingStore.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/version.h>
#include <appmodel.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <time.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <string>

#include "Globals.h"

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

AboutPage::AboutPage() {
  InitializeComponent();
  this->PopulateVersion();
  this->PopulateEvents();

  AddEventListener(
    gKneeboard->GetTroubleshootingStore()->evGameEventUpdated,
    &AboutPage::PopulateEvents,
    this);
}

void AboutPage::PopulateVersion() {
  std::string_view commitID(Version::CommitID);

  const auto version = fmt::format(
    "{}.{}.{}.{}-{}{}{}{}",
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build,
    commitID.substr(commitID.length() - 6),
    Version::HaveModifiedFiles ? "-dirty" : "",
    Version::IsGithubActionsBuild ? "-gha" : "-local",
#ifdef DEBUG
    "-debug"
#else
    ""
#endif
  );

  tm commitTime;
  gmtime_s(&commitTime, &Version::CommitUnixTimestamp);

  UINT32 packageNameLen = MAX_PATH;
  wchar_t packageName[MAX_PATH];
  if (
    GetCurrentPackageFullName(&packageNameLen, packageName) == ERROR_SUCCESS) {
    packageNameLen -= 1;// null terminator
  } else {
    packageNameLen = 0;
  }

  auto details = fmt::format(
    "OpenKneeboard v{}\n\n"
    "Copyright © 2021-2022 Frederick Emmott.\n\n"
    "With thanks to Paul 'Goldwolf' Whittingham for the logo and banner "
    "artwork.\n\n"
    "Package: {}\n"
    "Built at: {}\n"
    "Build type: {}-{}\n"
    "Commited at: {:%Y-%m-%dT%H:%M:%SZ}\n"
    "Commit ID: {}\n",
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
    Version::CommitID);
  if (Version::HaveModifiedFiles) {
    std::string files = Version::ModifiedFiles;
    details += "\nModified files:\n" + files;
  }
  VersionText().Text(winrt::to_hstring(details));

  mVersionClipboardData = details;
}

static void SetClipboardText(const std::string& text) {
  Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(winrt::to_hstring(text));
  Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
}

void AboutPage::OnCopyVersionDataClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(mVersionClipboardData);
}

void AboutPage::OnCopyGameEventsClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(mGameEventsClipboardData);
}

void AboutPage::PopulateEvents() {
  auto events = gKneeboard->GetTroubleshootingStore()->GetGameEvents();

  auto message = fmt::format("Updated at {}", std::chrono::system_clock::now());

  if (events.empty()) {
    message += "\n\nNo events.";
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

    message += fmt::format(
      "{}:\n"
      "  Latest value:  '{}'\n"
      "  First seen:    {}\n"
      "  Last seen:     {}\n"
      "  Receive count: {}\n"
      "  Change count:  {}",
      name,
      event.mValue,
      event.mFirstSeen,
      event.mLastSeen,
      event.mReceiveCount,
      event.mUpdateCount);
  }

  mGameEventsClipboardData = message;

  EventsText().Text(winrt::to_hstring(message));
}

}// namespace winrt::OpenKneeboardApp::implementation
