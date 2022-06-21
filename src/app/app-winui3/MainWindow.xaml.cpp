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

  gUIThreadDispatcherQueue = DispatcherQueue();
  gDXResources = DXResources::Create();
  gKneeboard = std::make_shared<KneeboardState>(mHwnd, gDXResources);

  OnTabsChanged();
  OnTabChanged();

  auto view = gKneeboard->GetPrimaryViewForDisplay();
  AddEventListener(
    view->evCurrentTabChangedEvent, &MainWindow::OnTabChanged, this);
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
}

MainWindow::~MainWindow() {
  gMainWindow = {};
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

void MainWindow::OnTabChanged() {
  const auto id
    = gKneeboard->GetPrimaryViewForDisplay()->GetCurrentTab()->GetRuntimeID();

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

  if (
    tabID
    == gKneeboard->GetPrimaryViewForDisplay()
         ->GetCurrentTab()
         ->GetRuntimeID()) {
    Frame().Navigate(xaml_typename<TabPage>(), tag);
    return;
  }

  gKneeboard->GetPrimaryViewForDisplay()->SetCurrentTabByID(
    Tab::RuntimeID::FromTemporaryValue(tabID));
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
    return;
  }
  if (path == L"Settings/Input") {
    Frame().Navigate(xaml_typename<InputSettingsPage>());
    return;
  }
  if (path == L"Settings/Tabs") {
    Frame().Navigate(xaml_typename<TabSettingsPage>());
    return;
  }
}

}// namespace winrt::OpenKneeboardApp::implementation
