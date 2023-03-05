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
#pragma once

#include <OpenKneeboard/Bookmark.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/IKneeboardView.h>

#include <memory>
#include <thread>

#include "MainWindow.g.h"
#include "WithPropertyChangedEvent.h"

using namespace winrt::Microsoft::UI::Dispatching;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Navigation;
using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {
struct MainWindow : MainWindowT<MainWindow>,
                    EventReceiver,
                    OpenKneeboard::WithPropertyChangedEvent {
  MainWindow();
  ~MainWindow();

  void OnNavigationItemInvoked(
    const IInspectable& sender,
    const NavigationViewItemInvokedEventArgs& args) noexcept;

  void OnBackRequested(
    const IInspectable& sender,
    const NavigationViewBackRequestedEventArgs&) noexcept;

  void UpdateTitleBarMargins(
    const IInspectable& sender,
    const IInspectable& args) noexcept;

  Windows::Foundation::Collections::IVector<IInspectable>
  NavigationItems() noexcept;

 private:
  struct NavigationTag {
    ITab::RuntimeID mTabID;
    std::optional<PageID> mPageID;

    IInspectable box() const;
    static NavigationTag unbox(IInspectable);

    constexpr auto operator<=>(const NavigationTag&) const = default;
  };

  winrt::apartment_context mUIThread;
  HWND mHwnd;
  winrt::handle mHwndFile;
  std::shared_ptr<IKneeboardView> mKneeboardView;

  FrameworkElement mProfileSwitcher {nullptr};

  std::vector<EventHandlerToken> mKneeboardViewEvents;
  DispatcherQueueController mDQC {nullptr};
  DispatcherQueueTimer mFrameTimer {nullptr};
  bool mSwitchingTabsFromNavSelection = false;

  winrt::fire_and_forget LaunchOpenKneeboardURI(std::string_view);
  winrt::fire_and_forget OnViewOrderChanged();
  winrt::fire_and_forget OnTabChanged() noexcept;
  winrt::fire_and_forget OnTabsChanged();
  winrt::fire_and_forget UpdateProfileSwitcherVisibility();
  winrt::fire_and_forget RenameTab(std::shared_ptr<ITab>);
  winrt::fire_and_forget
  RenameBookmark(std::shared_ptr<ITab>, Bookmark, winrt::hstring title);

  void SaveWindowPosition();

  winrt::Windows::Foundation::IAsyncAction OnClosed(
    const IInspectable&,
    const WindowEventArgs&) noexcept;
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
