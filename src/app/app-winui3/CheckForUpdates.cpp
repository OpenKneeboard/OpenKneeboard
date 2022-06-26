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
using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::Foundation;
using namespace winrt;

namespace OpenKneeboard {

IAsyncAction CheckForUpdates(const XamlRoot& xamlRoot) {
  winrt::apartment_context uiThread;

  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  auto settings = gKneeboard->GetAppSettings().mAutoUpdate;
  if (settings.mDisabledUntil > static_cast<uint64_t>(now)) {
    dprint("Not checking for update, too soon");
    co_return;
  }

  HttpClient http;
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
    // GetStringAsync is UTF-16, GetBufferAsync is often truncated
    auto stream = co_await http.GetInputStreamAsync(Uri(hstring {
      L"https://api.github.com/repos/OpenKneeboard/OpenKneeboard/releases"}));
    std::string json;
    Windows::Storage::Streams::Buffer buffer(4096);
    Windows::Storage::Streams::IBuffer readBuffer;
    do {
      readBuffer = co_await stream.ReadAsync(
        buffer,
        buffer.Capacity(),
        Windows::Storage::Streams::InputStreamOptions::Partial);
      json += std::string_view {
        reinterpret_cast<const char*>(readBuffer.data()), readBuffer.Length()};
    } while (readBuffer.Length() > 0);
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
    dialog.Title(box_value(to_hstring(_(L"Update OpenKneeboard"))));
    dialog.Content(box_value(to_hstring(std::format(
      _("A new version of OpenKneeboard is available; would you like to "
        "upgrade from {} to {}?"),
      oldName,
      newName))));
    dialog.PrimaryButtonText(_(L"Update Now"));
    dialog.SecondaryButtonText(_(L"Skip This Version"));
    dialog.CloseButtonText(_(L"Not Now"));
    dialog.DefaultButton(ContentDialogButton::Primary);

    result = co_await dialog.ShowAsync();
  }

  if (result == ContentDialogResult::Primary) {
    co_await uiThread;
    ProgressRing progressRing;
    TextBlock progressText;
    progressText.Text(_(L"Starting download..."));

    StackPanel layout;
    layout.Margin({12, 12, 12, 12});
    layout.Spacing(8);
    layout.Orientation(Orientation::Horizontal);
    layout.Children().Append(progressRing);
    layout.Children().Append(progressText);

    ContentDialog dialog;
    dialog.XamlRoot(xamlRoot);
    dialog.Title(box_value(to_hstring(_(L"Downloading Update"))));
    dialog.Content(layout);
    dialog.CloseButtonText(_(L"Cancel"));
    bool cancelled = false;
    auto dialogResultAsync = [](bool& cancelled, ContentDialog& dialog)
      -> decltype(dialog.ShowAsync()) {
      const auto result = co_await dialog.ShowAsync();
      if (result == ContentDialogResult::None) {
        cancelled = true;
      }
      co_return result;
    }(cancelled, dialog);

    // ProgressRing is buggy inside a ContentDialog, and only works properly
    // after it has been set multiple times with a time delay
    progressRing.IsIndeterminate(false);
    progressRing.IsActive(true);
    progressRing.Value(0);
    co_await winrt::resume_after(std::chrono::milliseconds(100));
    co_await uiThread;
    progressRing.Value(100);
    progressRing.IsIndeterminate(true);

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
      uint64_t totalBytes = 0;
      auto op = http.GetInputStreamAsync(Uri {to_hstring(msixURL)});
      op.Progress([&](auto&, const HttpProgress& hp) {
        if (hp.TotalBytesToReceive) {
          totalBytes = hp.TotalBytesToReceive.Value();
        }
      });
      auto stream = co_await op;
      Windows::Storage::Streams::Buffer buffer(4096);
      Windows::Storage::Streams::IBuffer readBuffer;
      uint64_t bytesRead = 0;
      do {
        readBuffer = co_await stream.ReadAsync(
          buffer,
          buffer.Capacity(),
          Windows::Storage::Streams::InputStreamOptions::Partial);

        of << std::string_view {
          reinterpret_cast<const char*>(readBuffer.data()),
          readBuffer.Length()};

        bytesRead += readBuffer.Length();
        co_await uiThread;
        progressRing.IsIndeterminate(false);
        progressRing.Value((100.0 * bytesRead) / totalBytes);
        progressText.Text(std::format(
          _(L"{:0.2f}MiB of {:0.2f}MiB"),
          bytesRead / (1024.0 * 1024),
          totalBytes / (1024.0 * 1024)));

      } while (readBuffer.Length() > 0 && !cancelled);
      of.close();
      if (cancelled) {
        if (std::filesystem::exists(tmpFile)) {
          std::filesystem::remove(tmpFile);
        }
        co_return;
      }
      std::filesystem::rename(tmpFile, destination);
    }

    co_await uiThread;
    dialog.Title(box_value(to_hstring(_(L"Installing Update"))));
    progressRing.IsIndeterminate(true);
    progressRing.IsActive(true);

    progressText.Text(_(L"Launching installer..."));

    co_await winrt::when_any(
      [](decltype(dialogResultAsync)& dialogResultAsync) -> IAsyncAction {
        co_await dialogResultAsync;
      }(dialogResultAsync),
      []() -> IAsyncAction {
        co_await resume_after(std::chrono::seconds(1));
      }());
    if (cancelled) {
      co_return;
    }
    settings.mForceUpgradeTo = {};
    auto app = gKneeboard->GetAppSettings();
    app.mAutoUpdate = settings;
    gKneeboard->SetAppSettings(app);
    OpenKneeboard::LaunchURI(to_utf8(destination));
    co_await uiThread;
    Application::Current().Exit();
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
