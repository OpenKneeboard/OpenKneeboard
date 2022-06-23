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
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/version.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "Globals.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace ::OpenKneeboard;

namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {
MainWindow::MainWindow() {
  InitializeComponent();

  {
    auto ref = get_strong();
    winrt::check_hresult(ref.as<IWindowNative>()->get_WindowHandle(&mHwnd));
    gMainWindow = mHwnd;
  }

  Title(L"OpenKneeboard");
  ExtendsContentIntoTitleBar(true);
  SetTitleBar(AppTitleBar());

  auto bigIcon = LoadImageW(
    GetModuleHandleW(nullptr),
    L"appIcon",
    IMAGE_ICON,
    GetSystemMetrics(SM_CXICON),
    GetSystemMetrics(SM_CYICON),
    0);
  SendMessage(mHwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
  auto smallIcon = LoadImageW(
    GetModuleHandleW(nullptr),
    L"appIcon",
    IMAGE_ICON,
    GetSystemMetrics(SM_CXSMICON),
    GetSystemMetrics(SM_CYSMICON),
    0);
  SendMessage(
    mHwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));

  gUIThreadDispatcherQueue = DispatcherQueue();
  gDXResources = DXResources::Create();
  gKneeboard = std::make_shared<KneeboardState>(mHwnd, gDXResources);

  OnTabsChanged();
  OnViewOrderChanged();

  AddEventListener(
    gKneeboard->evViewOrderChangedEvent, &MainWindow::OnViewOrderChanged, this);

  AddEventListener(
    gKneeboard->evTabsChangedEvent, &MainWindow::OnTabsChanged, this);

  // TODO: add to globals as 'game loop' thread
  mDQC = DispatcherQueueController::CreateOnDedicatedThread();
  mFrameTimer = mDQC.DispatcherQueue().CreateTimer();
  mFrameTimer.Interval(std::chrono::milliseconds(1000 / 60));
  mFrameTimer.Tick([=](auto&, auto&) { gKneeboard->evFrameTimerEvent.Emit(); });
  mFrameTimer.Start();

  auto settings = gKneeboard->GetAppSettings();
  if (settings.mWindowRect) {
    auto rect = *settings.mWindowRect;
    if (rect.top != -32000 && rect.left != -32000) {
      // GetWindowRect() returns (-32000, -32000) for some corner cases, at
      // least when minimized
      SetWindowPos(
        mHwnd,
        NULL,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        0);
    }
  }

  this->Closed({this, &MainWindow::OnClosed});

  auto hwndMappingName = std::format(L"Local\\{}.hwnd", ProjectNameW);
  // Initially leak for the duration of the app
  mHwndFile.attach(CreateFileMappingW(
    INVALID_HANDLE_VALUE,
    nullptr,
    PAGE_READWRITE,
    sizeof(HWND),
    sizeof(HWND),
    hwndMappingName.c_str()));
  if (!mHwndFile) {
    dprintf("Failed to open hwnd file: {:#x}", GetLastError());
    return;
  }
  void* mapping
    = MapViewOfFile(mHwndFile.get(), FILE_MAP_WRITE, 0, 0, sizeof(HWND));
  *reinterpret_cast<HWND*>(mapping) = mHwnd;
  UnmapViewOfFile(mapping);

  UpdateTitleBarMargins(nullptr, nullptr);

  RegisterURIHandler(
    "openkneeboard", [this](auto uri) { this->LaunchOpenKneeboardURI(uri); });
  this->CheckForUpdates();
}

MainWindow::~MainWindow() {
  gMainWindow = {};
}

void MainWindow::OnViewOrderChanged() {
  RemoveEventListener(mTabChangedEvent);
  mKneeboardView = gKneeboard->GetActiveViewForGlobalInput();

  mTabChangedEvent = AddEventListener(
    mKneeboardView->evCurrentTabChangedEvent, &MainWindow::OnTabChanged, this);
  this->OnTabChanged();
}

void MainWindow::UpdateTitleBarMargins(
  const IInspectable&,
  const IInspectable&) noexcept {
  auto titleBarMargin = AppTitleBar().Margin();
  auto titleMargin = AppTitle().Margin();

  const auto displayMode = Navigation().DisplayMode();
  const auto buttonWidth = Navigation().CompactPaneLength();

  titleBarMargin.Left = buttonWidth;
  titleMargin.Left = 4;

  if (displayMode == NavigationViewDisplayMode::Minimal) {
    titleBarMargin.Left = buttonWidth * 2;
  } else if (
    displayMode == NavigationViewDisplayMode::Expanded
    && !Navigation().IsPaneOpen()) {
    titleMargin.Left = 24;
  }

  AppTitleBar().Margin(titleBarMargin);
  AppTitle().Margin(titleMargin);
}

void MainWindow::SaveWindowPosition() {
  const bool isMinimized = IsIconic(mHwnd);
  if (isMinimized) {
    return;
  }

  WINDOWPLACEMENT placement {.length = sizeof(WINDOWPLACEMENT)};
  GetWindowPlacement(mHwnd, &placement);
  if (placement.showCmd == SW_SHOWMAXIMIZED) {
    return;
  }

  // Don't use rect from `placement` as that's workspace coords,
  // but we want screen coords
  RECT windowRect {};
  if (!GetWindowRect(mHwnd, &windowRect)) {
    return;
  }

  auto settings = gKneeboard->GetAppSettings();
  settings.mWindowRect = windowRect;
  gKneeboard->SetAppSettings(settings);
}

winrt::Windows::Foundation::IAsyncAction MainWindow::OnClosed(
  const IInspectable&,
  const WindowEventArgs&) noexcept {
  this->SaveWindowPosition();

  dprint("Stopping frame timer...");
  mFrameTimer.Stop();
  mFrameTimer = {nullptr};
  dprint("Stopping dispatch queue...");
  co_await mDQC.ShutdownQueueAsync();
  mDQC = {nullptr};
  dprint("Frame thread shutdown.");

  gKneeboard = {};
  gDXResources = {};
}

winrt::fire_and_forget MainWindow::OnTabChanged() {
  co_await mUIThread;
  const auto id = mKneeboardView->GetCurrentTab()->GetRuntimeID();

  for (auto it: this->Navigation().MenuItems()) {
    auto item = it.try_as<Control>();
    if (!item) {
      continue;
    }
    auto tag = item.Tag();
    if (!tag) {
      continue;
    }
    if (winrt::unbox_value<uint64_t>(tag) == id) {
      Navigation().SelectedItem(item);
      break;
    }
  }
  this->Frame().Navigate(
    xaml_typename<TabPage>(), winrt::box_value(id.GetTemporaryValue()));
}

void MainWindow::OnTabsChanged() {
  auto navItems = this->Navigation().MenuItems();
  navItems.Clear();
  for (auto tab: gKneeboard->GetTabs()) {
    muxc::NavigationViewItem item;
    item.Content(winrt::box_value(winrt::to_hstring(tab->GetTitle())));
    item.Tag(winrt::box_value(tab->GetRuntimeID().GetTemporaryValue()));

    auto glyph = tab->GetGlyph();
    if (!glyph.empty()) {
      muxc::FontIcon icon;
      icon.Glyph(winrt::to_hstring(glyph));
      item.Icon(icon);
    }

    navItems.Append(item);
  }
}

void MainWindow::OnNavigationItemInvoked(
  const IInspectable&,
  const NavigationViewItemInvokedEventArgs& args) noexcept {
  if (args.IsSettingsInvoked()) {
    Frame().Navigate(xaml_typename<SettingsPage>());
    return;
  }

  auto item = args.InvokedItemContainer().try_as<NavigationViewItem>();
  if (!item) {
    return;
  }

  if (item == AboutNavItem()) {
    Frame().Navigate(xaml_typename<AboutPage>());
    return;
  }

  auto tag = item.Tag();
  if (!tag) {
    return;
  }

  const auto tabID = winrt::unbox_value<uint64_t>(tag);

  if (tabID == mKneeboardView->GetCurrentTab()->GetRuntimeID()) {
    Frame().Navigate(xaml_typename<TabPage>(), tag);
    return;
  }

  mKneeboardView->SetCurrentTabByID(Tab::RuntimeID::FromTemporaryValue(tabID));
}

void MainWindow::OnBackRequested(
  const IInspectable&,
  const NavigationViewBackRequestedEventArgs&) noexcept {
  Frame().GoBack();
}

winrt::fire_and_forget MainWindow::LaunchOpenKneeboardURI(
  std::string_view uriStr) {
  auto uri = winrt::Windows::Foundation::Uri(winrt::to_hstring(uriStr));
  std::wstring_view path(uri.Path());

  if (path.starts_with(L'/')) {
    path.remove_prefix(1);
  }

  co_await mUIThread;

  if (path == L"Settings/Games") {
    Frame().Navigate(xaml_typename<GameSettingsPage>());
    co_return;
  }
  if (path == L"Settings/Input") {
    Frame().Navigate(xaml_typename<InputSettingsPage>());
    co_return;
  }
  if (path == L"Settings/Tabs") {
    Frame().Navigate(xaml_typename<TabSettingsPage>());
    co_return;
  }
}

winrt::fire_and_forget MainWindow::CheckForUpdates() {
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

  auto release = isPreRelease ? latestRelease : latestStableRelease;
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
    muxc::ContentDialog dialog;
    dialog.XamlRoot(this->Content().XamlRoot());
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

}// namespace winrt::OpenKneeboardApp::implementation
