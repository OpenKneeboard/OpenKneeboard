// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "MainWindow.g.h"
#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/Bookmark.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/Handles.hpp>
#include <OpenKneeboard/KneeboardView.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/single_threaded_lockable.hpp>

#include <memory>
#include <thread>

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

  [[nodiscard]]
  task<void> Init();

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

    constexpr bool operator==(const NavigationTag&) const = default;
  };
  winrt::Windows::Foundation::Collections::IVector<
    winrt::Windows::Foundation::IInspectable>
    mNavigationItems {nullptr};

  winrt::apartment_context mUIThread;
  HWND mHwnd;
  winrt::handle mHwndFile;
  std::shared_ptr<KneeboardView> mKneeboardView;
  unique_hwineventhook mWinEventHook;

  FrameworkElement mProfileSwitcher {nullptr};

  std::vector<EventHandlerToken> mKneeboardViewEvents;
  std::stop_source mFrameLoopStopSource;
  std::optional<task<void>> mFrameLoop;

  enum class TabSwitchReason {
    InAppNavSelection,
    ProfileSwitched,
    Other,
  };
  TabSwitchReason mTabSwitchReason {TabSwitchReason::Other};

  // Used to track whether or not we've shown an elevation warning
  DWORD mElevatedConsumerProcessID {};

  void Show();

  // Used instead of a timer so there is always a positive amount
  // of time between iterations; this keeps the UI responsive, and
  // avoids building up a massive backlog of overdue scheduled
  // events.
  task<void> FrameLoop();
  task<void> FrameTick(std::chrono::steady_clock::time_point nextFrameAt);
  single_threaded_lockable mFrameInProgress;

  std::vector<EventHandlerToken> mTabsEvents;

  OpenKneeboard::fire_and_forget LaunchOpenKneeboardURI(std::string_view);
  OpenKneeboard::fire_and_forget OnTabChanged() noexcept;
  OpenKneeboard::fire_and_forget OnTabsChanged();
  OpenKneeboard::fire_and_forget OnTabSettingsChanged(std::shared_ptr<ITab>);
  OpenKneeboard::fire_and_forget OnAPIEvent(APIEvent);
  OpenKneeboard::fire_and_forget OnLoaded();
  task<void> PromptForViewMode();
  task<void> ShowWarningIfTabletConfiguredButUnusable();
  task<void> ShowWarningIfWintabConfiguredButUnusable();
  task<void> ShowWarningIfOTDIPCConfiguredButUnusable();
  OpenKneeboard::fire_and_forget UpdateProfileSwitcherVisibility();
  OpenKneeboard::fire_and_forget RenameTab(std::shared_ptr<ITab>);
  OpenKneeboard::fire_and_forget Shutdown();

  void CheckForElevatedConsumer();
  OpenKneeboard::fire_and_forget ShowWarningIfElevated(DWORD pid);

  OpenKneeboard::fire_and_forget
  RenameBookmark(std::shared_ptr<ITab>, Bookmark, winrt::hstring title);

  void SaveWindowPosition();
  std::optional<RECT> mWindowPosition;

  static std::filesystem::path GetInstanceDataPath();
  task<void> WriteInstanceData();

  static LRESULT SubclassProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData);

  static void WinEventProc(
    HWINEVENTHOOK,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime);

  audited_ptr<DXResources> mDXR;
  std::shared_ptr<KneeboardState> mKneeboard;

  void ResetKneeboardView();
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
