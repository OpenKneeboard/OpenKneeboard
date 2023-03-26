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

#include "Globals.h"

#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/LaunchURI.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/version.h>

#include <shims/filesystem>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <format>
#include <fstream>
#include <regex>

#include <semver200.h>
#include <shobjidl.h>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::Foundation;
using namespace winrt;

namespace OpenKneeboard {

static std::string ToSemVerString(std::string_view raw) {
  // Remove leading v
  std::string ret {(raw.front() == 'v') ? raw.substr(1) : raw};
  // The .z in x.y.z is mandatory
  ret = std::regex_replace(
    ret,
    std::regex {"^(\\d+\\.\\d+)(-|$)"},
    "\\1.0\\2",
    std::regex_constants::format_sed);
  // 'beta3' should be 'beta.3' to get numeric comparisons
  ret = std::regex_replace(
    ret,
    std::regex {"-([a-z]+)(\\d+)\\b"},
    "-\\1.\\2",
    std::regex_constants::format_sed);
  return ret;
}

static fire_and_forget ShowResultDialog(
  std::string_view message,
  winrt::apartment_context& uiThread,
  const XamlRoot& xamlRoot);

concurrency::task<UpdateResult> CheckForUpdates(
  UpdateCheckType checkType,
  const XamlRoot& xamlRoot) {
  winrt::apartment_context uiThread;

  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();

  auto appSettings = gKneeboard->GetAppSettings();
  {
    const auto updateLogVersion = std::format(
      "v{}.{}.{}.{} ('{}')",
      Version::Major,
      Version::Minor,
      Version::Patch,
      Version::Build,
      Version::ReleaseName);
    if (updateLogVersion != appSettings.mLastRunVersion) {
      RegSetKeyValueA(
        HKEY_CURRENT_USER,
        winrt::to_string(std::format(L"{}\\Updates", RegistrySubKey)).c_str(),
        std::to_string(now).c_str(),
        REG_SZ,
        updateLogVersion.data(),
        static_cast<DWORD>(updateLogVersion.size()));
      appSettings.mLastRunVersion = updateLogVersion;
      gKneeboard->SetAppSettings(appSettings);
    }
  }

  AutoUpdateSettings settings = appSettings.mAutoUpdate;
  if (checkType == UpdateCheckType::Manual) {
    settings.mDisabledUntil = 0;
    settings.mSkipVersion = {};
  }
  auto& testing = settings.mTesting;
  if (testing.mAlwaysCheck) {
    settings.mDisabledUntil = 0;
  }

  if (settings.mDisabledUntil > static_cast<uint64_t>(now)) {
    dprint("Not checking for update, too soon");
    co_return UpdateResult::NotInstallingUpdate;
  }

  if (
    settings.mChannel == AutoUpdateSettings::StableChannel
    && !Version::IsStableRelease) {
    settings.mChannel = AutoUpdateSettings::PreviewChannel;
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
       Version::IsGithubActionsBuild ? L"GHA" : L"local")});
  headers.Accept().Clear();
  headers.Accept().Append(
    Windows::Web::Http::Headers::HttpMediaTypeWithQualityHeaderValue(
      hstring {L"application/vnd.github.v3+json"}));

  const auto baseUri = testing.mBaseURI.empty()
    ? "https://autoupdate.openkneeboard.com"
    : testing.mBaseURI;

  const auto uri = std::format("{}/{}-msi.json", baseUri, settings.mChannel);
  dprintf("Starting update check: {}", uri);

  nlohmann::json releases;
  try {
    // GetStringAsync is UTF-16, GetBufferAsync is often truncated
    auto stream = co_await http.GetInputStreamAsync(Uri(to_hstring(uri)));
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
    const auto message = "Buffering error, or invalid JSON from GitHub API";
    dprint(message);
    if (checkType == UpdateCheckType::Manual) {
      ShowResultDialog(message, uiThread, xamlRoot);
    }
    co_return UpdateResult::NotInstallingUpdate;
  } catch (const winrt::hresult_error& hr) {
    const auto message = std::format(
      "Error fetching releases from GitHub API: {} ({:#08x}) - {}",
      hr.code().value,
      std::bit_cast<uint32_t>(hr.code().value),
      to_string(hr.message()));
    dprint(message);
    if (checkType == UpdateCheckType::Manual) {
      ShowResultDialog(message, uiThread, xamlRoot);
    }
    co_return UpdateResult::NotInstallingUpdate;
  }
  if (releases.empty()) {
    dprint("Didn't get any releases from github API :/");
    if (checkType == UpdateCheckType::Manual) {
      ShowResultDialog(
        _("Error: API did not return any releases."), uiThread, xamlRoot);
    }
    co_return UpdateResult::NotInstallingUpdate;
  }

  auto latestRelease = releases.front();
  dprintf(
    "Latest release is {}", latestRelease.at("name").get<std::string_view>());

  scope_guard oncePerDay([&]() {
    settings.mDisabledUntil = now + (60 * 60 * 24);
    appSettings.mAutoUpdate = settings;
    gKneeboard->SetAppSettings(appSettings);
  });

  const auto currentVersionString = ToSemVerString(
    testing.mFakeCurrentVersion.empty() ? Version::ReleaseName
                                        : testing.mFakeCurrentVersion);
  const auto latestVersionString = ToSemVerString(
    testing.mFakeUpdateVersion.empty()
      ? latestRelease.at("tag_name").get<std::string_view>()
      : testing.mFakeUpdateVersion);

  const version::Semver200_version currentVersion(currentVersionString);
  const version::Semver200_version latestVersion(latestVersionString);

  if (currentVersion >= latestVersion) {
    dprintf(
      "Current version '{}' >= latest '{}'",
      currentVersionString,
      latestVersionString);
    if (checkType == UpdateCheckType::Manual) {
      ShowResultDialog(
        _("You're running the latest version!"), uiThread, xamlRoot);
    }
    co_return UpdateResult::NotInstallingUpdate;
  } else {
    dprintf(
      "Current version '{}' < latest '{}'",
      currentVersionString,
      latestVersionString);
  }

  const auto oldName = Version::ReleaseName;
  const auto newName = latestRelease.at("tag_name").get<std::string_view>();

  dprintf("Found upgrade {} to {}", oldName, newName);
  if (newName == settings.mSkipVersion) {
    dprintf("Skipping {} at user request.", newName);
    co_return UpdateResult::NotInstallingUpdate;
  }
  const auto assets = latestRelease.at("assets");
  auto updateAsset = std::ranges::find_if(assets, [=](const auto& asset) {
    auto name = asset.at("name").get<std::string_view>();
    return name.ends_with(".msix") && (name.find("Debug") == name.npos);
  });
  if (updateAsset == assets.end()) {
    dprint("Didn't find any MSIX");
    updateAsset = std::ranges::find_if(assets, [=](const auto& asset) {
      auto name = asset.at("name").get<std::string_view>();
      return name.ends_with(".msi") && (name.find("Debug") == name.npos);
    });
    if (updateAsset == assets.end()) {
      dprint("Didn't find any MSI");
      if (checkType == UpdateCheckType::Manual) {
        ShowResultDialog(
          std::format(
            _("Error: found a new version ({}), but couldn't find an "
              "installer"),
            newName),
          uiThread,
          xamlRoot);
      }
      co_return UpdateResult::NotInstallingUpdate;
    }
  }
  const auto updateURL
    = updateAsset->at("browser_download_url").get<std::string_view>();
  dprintf("Update installer found at {}", updateURL);

  ContentDialogResult result;
  {
    HyperlinkButton releaseNotesLink;
    releaseNotesLink.Content(box_value(
      to_hstring(std::format(_("OpenKneeboard {} is available!"), newName))));
    releaseNotesLink.NavigateUri(
      Uri {to_hstring(latestRelease.at("html_url").get<std::string_view>())});
    releaseNotesLink.HorizontalAlignment(HorizontalAlignment::Center);
    TextBlock promptText;
    promptText.Text(to_hstring(
      std::format(_("Would you like to upgrade from {}?"), oldName)));
    promptText.TextWrapping(TextWrapping::WrapWholeWords);

    StackPanel layout;
    layout.Margin({8, 8, 8, 8});
    layout.Spacing(4);
    layout.Orientation(Orientation::Vertical);
    layout.Children().Append(releaseNotesLink);
    layout.Children().Append(promptText);

    ContentDialog dialog;
    dialog.XamlRoot(xamlRoot);
    dialog.Title(box_value(to_hstring(_(L"Update OpenKneeboard"))));
    dialog.Content(layout);
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

    const auto destination = Filesystem::GetTemporaryDirectory()
      / updateAsset->at("name").get<std::string_view>();
    if (!std::filesystem::exists(destination)) {
      auto tmpFile = destination;
      tmpFile += ".download";
      if (std::filesystem::exists(tmpFile)) {
        std::filesystem::remove(tmpFile);
      }
      std::ofstream of(tmpFile, std::ios::binary | std::ios::out);
      uint64_t totalBytes = 0;
      auto op = http.GetInputStreamAsync(Uri {to_hstring(updateURL)});
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
        co_return UpdateResult::NotInstallingUpdate;
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
      co_return UpdateResult::NotInstallingUpdate;
    }
    // Settings saved by scope guard
    co_await uiThread;
    co_await OpenKneeboard::LaunchURI(to_utf8(destination));
    Application::Current().Exit();
    co_return UpdateResult::InstallingUpdate;
  }

  if (result == ContentDialogResult::Secondary) {
    // "Skip This Version"
    settings.mSkipVersion = newName;
  }
  // Settings saved by scope guard
  co_return UpdateResult::NotInstallingUpdate;
}

static fire_and_forget ShowResultDialog(
  std::string_view message,
  winrt::apartment_context& uiThread,
  const XamlRoot& xamlRoot) {
  co_await uiThread;

  ContentDialog dialog;
  dialog.XamlRoot(xamlRoot);
  dialog.Title(box_value(to_hstring(_(L"Update OpenKneeboard"))));
  dialog.Content(box_value(to_hstring(message)));
  dialog.CloseButtonText(_(L"OK"));
  dialog.DefaultButton(ContentDialogButton::Close);
  co_await dialog.ShowAsync();
}

}// namespace OpenKneeboard
