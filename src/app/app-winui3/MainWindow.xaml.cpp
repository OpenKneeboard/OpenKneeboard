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

  {
    auto ref = get_strong();
    winrt::check_hresult(ref.as<IWindowNative>()->get_WindowHandle(&mHwnd));
  }
  gUIThreadDispatcherQueue = winrt::make_weak(DispatcherQueue());
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
}

MainWindow::~MainWindow() {
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

void MainWindow::OnNavigationSelectionChanged(
  const IInspectable&,
  const NavigationViewSelectionChangedEventArgs& args) {
  auto item = args.SelectedItem().try_as<NavigationViewItem>();
  if (!item) {
    return;
  }
  auto tag = item.Tag();
  if (!tag) {
    return;
  }
  gKneeboard->SetTabID(winrt::unbox_value<uint64_t>(tag));
}

}// namespace winrt::OpenKneeboardApp::implementation
