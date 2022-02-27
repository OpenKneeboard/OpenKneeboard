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
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabState.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/windows.ui.xaml.interop.h>

#include <nlohmann/json.hpp>

#include "Globals.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace ::OpenKneeboard;

namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {
MainWindow::MainWindow() {
  InitializeComponent();
  Title(L"OpenKneeboard");
  // See:
  // * https://docs.microsoft.com/en-us/windows/winui/api/microsoft.ui.xaml.window.settitlebar
  // * https://docs.microsoft.com/en-us/windows/apps/develop/title-bar?tabs=wasdk
  ExtendsContentIntoTitleBar(true);
  SetTitleBar(AppTitleBar());

  {
    auto ref = get_strong();
    winrt::check_hresult(ref.as<IWindowNative>()->get_WindowHandle(&mHwnd));
  }

  gUIThreadDispatcherQueue = DispatcherQueue();
  gDXResources = DXResources::Create();
  gKneeboard = std::make_shared<KneeboardState>(mHwnd, gDXResources);

  auto navItems = this->Navigation().MenuItems();
  for (auto tab: gKneeboard->GetTabs()) {
    muxc::NavigationViewItem item;
    item.Content(
      winrt::box_value(winrt::to_hstring(tab->GetTab()->GetTitle())));
    item.Tag(winrt::box_value(tab->GetInstanceID()));
    navItems.Append(item);
  }
  OnTabChanged();

  AddEventListener(
    gKneeboard->evCurrentTabChangedEvent, &MainWindow::OnTabChanged, this);

  // TODO: add to globals as 'game loop' thread
  mDQC = DispatcherQueueController::CreateOnDedicatedThread();
  mFrameTimer = mDQC.DispatcherQueue().CreateTimer();
  mFrameTimer.Interval(std::chrono::milliseconds(1000 / 60));
  mFrameTimer.Tick([=](auto&, auto&) { gKneeboard->evFrameTimerEvent.Emit(); });
  mFrameTimer.Start();

  // TODO: install DCS hooks
}

MainWindow::~MainWindow() {
  mFrameTimer.Stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  gKneeboard = {};
  gDXResources = {};
}

void MainWindow::OnTabChanged() {
  const auto id = gKneeboard->GetCurrentTab()->GetInstanceID();

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
  this->Frame().Navigate(xaml_typename<TabPage>(), winrt::box_value(id));
}

void MainWindow::OnNavigationItemInvoked(
  const IInspectable&,
  const NavigationViewItemInvokedEventArgs& args) {

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

  if (tabID == gKneeboard->GetCurrentTab()->GetInstanceID()) {
    Frame().Navigate(xaml_typename<TabPage>(), tag);
    return;
  }

  gKneeboard->SetTabID(tabID);
}

}// namespace winrt::OpenKneeboardApp::implementation
