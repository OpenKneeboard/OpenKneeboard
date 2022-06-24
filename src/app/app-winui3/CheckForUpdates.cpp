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
#include "CheckForUpdates.h"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/version.h>
#include <shobjidl.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <format>
#include <fstream>
#include <shims/filesystem>

#include "Globals.h"

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt;

namespace OpenKneeboard {

winrt::Windows::Foundation::IAsyncAction CheckForUpdates(
  const XamlRoot& xamlRoot) {
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  auto settings = gKneeboard->GetAppSettings().mAutoUpdate;
  if (settings.mDisabledUntil > static_cast<uint64_t>(now)) {
    dprint("Not checking for update, too soon");
    co_return;
  }

  Windows::Web::Http::HttpClient http;
  auto headers = http.DefaultRequestHeaders();
  headers.UserAgent().Append(
    {ProjectNameW,
     std::format(
       L"{}.{}.{}.{}-{}",
       Version::Major,
       Version::Minor,
       Version::Patch,
       Version::Build,
       std::wstring_view {to_hstring(Version::CommitID)})});
  headers.Accept().Clear();
  headers.Accept().Append(
    Windows::Web::Http::Headers::HttpMediaTypeWithQualityHeaderValue(
      hstring {L"application/vnd.github.v3+json"}));

  dprint("Starting update check");

  nlohmann::json releases;
  try {
    auto json = co_await http.GetBufferAsync(Windows::Foundation::Uri(hstring {
      L"https://api.github.com/repos/OpenKneeboard/OpenKneeboard/releases"}));
    releases = nlohmann::json::parse(json.data());
  } catch (const nlohmann::json::parse_error&) {
    dprint("Buffering error, or invalid JSON from GitHub API");
    co_return;
  } catch (const winrt::hresult_error& hr) {
    dprintf(
      L"Error fetching releases from GitHub API: {} ({:#08x}) - {}",
      hr.code().value,
      std::bit_cast<uint32_t>(hr.code().value),
      hr.message());
    co_return;
  }
  if (releases.empty()) {
    dprint("Didn't get any releases from github API :/");
    co_return;
  }

  bool isPreRelease = false;
  nlohmann::json latestRelease;
  nlohmann::json latestStableRelease;
  nlohmann::json forcedRelease;

  for (auto release: releases) {
    const auto name = release.at("tag_name").get<std::string_view>();
    if (name == settings.mForceUpgradeTo) {
      forcedRelease = release;
      dprintf("Found forced release {}", name);
      continue;
    }

    if (latestRelease.is_null()) {
      latestRelease = release;
      dprintf("Latest release is {}", name);
    }
    if (
      latestStableRelease.is_null() && !release.at("prerelease").get<bool>()) {
      latestStableRelease = release;
      dprintf("Latest stable release is {}", name);
    }

    if (
      Version::CommitID
      == release.at("target_commitish").get<std::string_view>()) {
      isPreRelease = release.at("prerelease").get<bool>();
      dprintf(
        "Found this version as {} ({})",
        name,
        isPreRelease ? "prerelease" : "stable");
      if (settings.mForceUpgradeTo.empty()) {
        break;
      }
    }
  }

  settings.mHaveUsedPrereleases = settings.mHaveUsedPrereleases || isPreRelease;

  auto release
    = settings.mHaveUsedPrereleases ? latestRelease : latestStableRelease;
  if (!settings.mForceUpgradeTo.empty()) {
    release = forcedRelease;
  }

  if (release.is_null()) {
    dprint("Didn't find a valid release.");
    co_return;
  }

  auto newCommit = release.at("target_commitish").get<std::string_view>();

  if (newCommit == Version::CommitID) {
    dprintf("Up to date on commit ID {}", Version::CommitID);
    co_return;
  }

  const auto oldName = Version::ReleaseName;
  const auto newName = release.at("tag_name").get<std::string_view>();

  dprintf("Found upgrade {} to {}", oldName, newName);
  if (newName == settings.mSkipVersion) {
    dprint("Skipping update at user request.");
    co_return;
  }
  const auto assets = release.at("assets");
  const auto msixAsset = std::ranges::find_if(assets, [=](const auto& asset) {
    auto name = asset.at("name").get<std::string_view>();
    return name.ends_with(".msix") && (name.find("Debug") == name.npos);
  });
  if (msixAsset == assets.end()) {
    dprint("Didn't find any MSIX");
    co_return;
  }
  auto msixURL = msixAsset->at("browser_download_url").get<std::string_view>();
  dprintf("MSIX found at {}", msixURL);

  ContentDialogResult result;
  {
    ContentDialog dialog;
    dialog.XamlRoot(xamlRoot);
    dialog.Title(box_value(to_hstring(_("Update OpenKneeboard")))),
      dialog.Content(box_value(to_hstring(std::format(
        _("A new version of OpenKneeboard is available; would you like to "
          "upgrade from {} to {}?"),
        oldName,
        newName))));
    dialog.PrimaryButtonText(to_hstring(_("Update Now")));
    dialog.SecondaryButtonText(to_hstring(_("Skip This Version")));
    dialog.CloseButtonText(to_hstring(_("Not Now")));
    dialog.DefaultButton(ContentDialogButton::Primary);

    result = co_await dialog.ShowAsync();
  }

  if (result == ContentDialogResult::Primary) {
    co_await resume_background();

    wchar_t tempPath[MAX_PATH];
    auto tempPathLen = GetTempPathW(MAX_PATH, tempPath);

    const auto destination
      = std::filesystem::path(std::wstring_view {tempPath, tempPathLen})
      / msixAsset->at("name").get<std::string_view>();
    if (!std::filesystem::exists(destination)) {
      auto tmpFile = destination;
      tmpFile += ".download";
      if (std::filesystem::exists(tmpFile)) {
        std::filesystem::remove(tmpFile);
      }
      std::ofstream of(tmpFile, std::ios::binary | std::ios::out);
      auto stream = co_await http.GetInputStreamAsync(
        Windows::Foundation::Uri {to_hstring(msixURL)});
      Windows::Storage::Streams::Buffer buffer(4096);
      Windows::Storage::Streams::IBuffer readBuffer;
      do {
        readBuffer = co_await stream.ReadAsync(
          buffer,
          buffer.Capacity(),
          Windows::Storage::Streams::InputStreamOptions::Partial);
        of << std::string_view {
          reinterpret_cast<const char*>(readBuffer.data()),
          readBuffer.Length()};
      } while (readBuffer.Length() > 0);
      of.close();
      std::filesystem::rename(tmpFile, destination);
    }
    settings.mForceUpgradeTo = {};
    auto app = gKneeboard->GetAppSettings();
    app.mAutoUpdate = settings;
    gKneeboard->SetAppSettings(app);
    OpenKneeboard::LaunchURI(to_utf8(destination));
    co_return;
  }

  if (result == ContentDialogResult::Secondary) {
    // "Skip This Version"
    settings.mSkipVersion = newName;
  } else if (result == ContentDialogResult::None) {
    settings.mDisabledUntil = now + (60 * 60 * 48);
  }

  auto app = gKneeboard->GetAppSettings();
  app.mAutoUpdate = settings;
  gKneeboard->SetAppSettings(app);
}

}// namespace OpenKneeboard
