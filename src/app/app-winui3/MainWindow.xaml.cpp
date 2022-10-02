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

#include <OpenKneeboard/GetMainHWND.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/version.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

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

  gDXResources = DXResources::Create();
  gKneeboard = std::make_shared<KneeboardState>(mHwnd, gDXResources);

  OnTabsChanged();
  OnViewOrderChanged();

  AddEventListener(
    gKneeboard->evViewOrderChangedEvent, &MainWindow::OnViewOrderChanged, this);

  AddEventListener(
    gKneeboard->GetTabsList()->evTabsChangedEvent,
    &MainWindow::OnTabsChanged,
    this);

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
    /* high bits of size = */ 0,
    sizeof(MainWindowInfo),
    hwndMappingName.c_str()));
  if (!mHwndFile) {
    const auto err = GetLastError();
    dprintf(
      "Failed to open hwnd file: {} {:#08x}",
      err,
      std::bit_cast<uint32_t>(err));
    return;
  }
  void* mapping = MapViewOfFile(
    mHwndFile.get(), FILE_MAP_WRITE, 0, 0, sizeof(MainWindowInfo));
  *reinterpret_cast<MainWindowInfo*>(mapping) = {
    .mHwnd = mHwnd,
    .mVersion
    = {Version::Major, Version::Minor, Version::Patch, Version::Build},
  };
  UnmapViewOfFile(mapping);

  UpdateTitleBarMargins(nullptr, nullptr);

  RegisterURIHandler(
    "openkneeboard", [this](auto uri) { this->LaunchOpenKneeboardURI(uri); });

  mProfileSwitcher = this->ProfileSwitcher();
  this->UpdateProfileSwitcherVisibility();
  AddEventListener(
    gKneeboard->evProfileSettingsChangedEvent,
    &MainWindow::UpdateProfileSwitcherVisibility,
    this);
}

MainWindow::~MainWindow() {
  gMainWindow = {};
}

void MainWindow::UpdateProfileSwitcherVisibility() {
  // As of Windows App SDK v1.1.4, changing the visibility and signalling the
  // bound property changed doesn't correctly update the navigation view, even
  // if UpdateLayout() is also called, so we need to manually add/remove the
  // item

  auto footerItems = Navigation().FooterMenuItems();
  auto first = footerItems.First().Current();

  const auto settings = gKneeboard->GetProfileSettings();
  if (!settings.mEnabled) {
    Navigation().PaneCustomContent({nullptr});
    return;
  }
  Navigation().PaneCustomContent(mProfileSwitcher);

  auto uiProfiles = ProfileSwitcherFlyout().Items();
  uiProfiles.Clear();
  for (const auto& profile: settings.GetSortedProfiles()) {
    ToggleMenuFlyoutItem item;
    auto wname = to_hstring(profile.mName);
    item.Text(wname);
    item.Tag(box_value(to_hstring(profile.mID)));
    uiProfiles.Append(item);

    if (profile.mID == settings.mActiveProfile) {
      item.IsChecked(true);
      ProfileSwitcherLabel().Text(wname);

      ToolTip tooltip;
      tooltip.Content(
        box_value(std::format(_(L"Switch profiles - current is '{}'"), wname)));
      ToolTipService::SetToolTip(ProfileSwitcher(), tooltip);
    }
  }
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
  this->RemoveAllEventListeners();

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

winrt::fire_and_forget MainWindow::OnTabChanged() noexcept {
  co_await mUIThread;

  // Don't automatically move away from "About" or "Settings"
  auto currentItem = Navigation().SelectedItem();
  if (currentItem) {
    if (currentItem == Navigation().SettingsItem()) {
      co_return;
    }
    for (const auto& item: Navigation().FooterMenuItems()) {
      if (currentItem == item) {
        co_return;
      }
    }
  }

  const auto tab = mKneeboardView->GetCurrentTab();
  if (!tab) {
    co_return;
  }

  const auto id = tab->GetRuntimeID();

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
  for (auto tab: gKneeboard->GetTabsList()->GetTabs()) {
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

  if (item == HelpNavItem()) {
    Frame().Navigate(xaml_typename<HelpPage>());
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

  mKneeboardView->SetCurrentTabByID(ITab::RuntimeID::FromTemporaryValue(tabID));
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

  if (path == L"TeachingTips/ProfileSwitcher") {
    ProfileSwitcherTeachingTip().Target(mProfileSwitcher);
    ProfileSwitcherTeachingTip().IsOpen(true);
    co_return;
  }
}

}// namespace winrt::OpenKneeboardApp::implementation
