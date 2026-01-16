// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
// clang-format on

#include "App.xaml.h"
#include "CheckDCSHooks.h"
#include "CheckForUpdates.h"
#include "Globals.h"
#include "InstallPlugin.h"
#include "TabPage.xaml.h"

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/GetMainHWND.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/InterprocessRenderer.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/LaunchURI.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabletInputAdapter.hpp>
#include <OpenKneeboard/TabsList.hpp>
#include <OpenKneeboard/TryEnqueue.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/json.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/task/resume_after.hpp>
#include <OpenKneeboard/task/resume_on_signal.hpp>
#include <OpenKneeboard/tracing.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/winrt/Microsoft.UI.Interop.h>

#include <Shellapi.h>

#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.h>

#include <wil/resource.h>

#include <microsoft.ui.xaml.window.h>

#include <fstream>
#include <mutex>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace ::OpenKneeboard;

namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

using TabPage_WinRT = winrt::OpenKneeboardApp::TabPage;
using TabPage_Implementation = winrt::OpenKneeboardApp::implementation::TabPage;

namespace winrt::OpenKneeboardApp::implementation {
MainWindow::MainWindow() : mDXR(new DXResources()) {
  InitializeComponent();
  gDXResources.copy_from(mDXR);

  {
    auto ref = get_strong();
    winrt::check_hresult(ref.as<IWindowNative>()->get_WindowHandle(&mHwnd));
    gMainWindow = mHwnd;
  }

  Title(L"OpenKneeboard");
  ExtendsContentIntoTitleBar(true);
  SetTitleBar(AppTitleBar());
  Closed([this](const auto&, const auto&) { this->Shutdown(); });

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
}

task<void> MainWindow::Init() {
  mKneeboard = co_await KneeboardState::Create(mHwnd, mDXR);
  gKneeboard = mKneeboard;

  ResetKneeboardView();

  AddEventListener(
    mKneeboard->evActiveViewChangedEvent,
    std::bind_front(&MainWindow::ResetKneeboardView, this));
  if (!IsElevated()) {
    AddEventListener(mKneeboard->evGameChangedEvent, [this](DWORD pid, auto) {
      if (pid) {
        // While OpenXR will create an elevated consumer, for injection-based
        // approaches, injection will fail, so we need to catch them here
        this->ShowWarningIfElevated(pid);
      }
    });
  }

  AddEventListener(
    mKneeboard->GetTabsList()->evTabsChangedEvent,
    std::bind_front(&MainWindow::OnTabsChanged, this));

  AddEventListener(
    mKneeboard->evAPIEvent, std::bind_front(&MainWindow::OnAPIEvent, this));

  RootGrid().Loaded([this](const auto&, const auto&) { this->OnLoaded(); });

  auto settings = mKneeboard->GetAppSettings();
  if (settings.mWindowRect) {
    auto rect = *settings.mWindowRect;
    if (
      MonitorFromPoint({rect.left, rect.top}, MONITOR_DEFAULTTONULL)
      && MonitorFromPoint({rect.right, rect.bottom}, MONITOR_DEFAULTTONULL)) {
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

  const auto hwndMappingName
    = std::format("Local\\{}.hwnd", ProjectReverseDomainA);
  // Initially leak for the duration of the app
  auto hwndFile = Win32::UTF8::CreateFileMapping(
    INVALID_HANDLE_VALUE,
    nullptr,
    PAGE_READWRITE,
    /* high bits of size = */ 0,
    sizeof(MainWindowInfo),
    hwndMappingName);
  if (hwndFile.has_value()) {
    mHwndFile = std::move(hwndFile).value();
  } else {
    OPENKNEEBOARD_BREAK;
    dprint.Error(
      "Failed to open hwnd file: {} {:#010x}",
      hwndMappingName,
      static_cast<uint32_t>(hwndFile.error()));
    co_return;
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

  RegisterURIHandler(SpecialURIs::Scheme, [this](auto uri) {
    this->LaunchOpenKneeboardURI(uri);
  });

  mProfileSwitcher = this->ProfileSwitcher();
  this->UpdateProfileSwitcherVisibility();
  AddEventListener(
    mKneeboard->evProfileSettingsChangedEvent,
    std::bind_front(&MainWindow::UpdateProfileSwitcherVisibility, this));
  AddEventListener(
    mKneeboard->evSettingsChangedEvent,
    std::bind_front(&MainWindow::ResetKneeboardView, this));

  AddEventListener(
    mKneeboard->evCurrentProfileChangedEvent,
    [](auto self) -> ::OpenKneeboard::fire_and_forget {
      self->mTabSwitchReason = TabSwitchReason::ProfileSwitched;
      self->ResetKneeboardView();
      auto backStack = self->Frame().BackStack();
      std::vector<PageStackEntry> newBackStack;
      for (auto entry: backStack) {
        if (
          entry.SourcePageType().Name == xaml_typename<TabPage_WinRT>().Name) {
          newBackStack.clear();
          continue;
        }
        newBackStack.push_back(entry);
      }
      backStack.ReplaceAll(newBackStack);
      co_await self->PromptForViewMode();
    } | bind_refs_front(this)
      | bind_winrt_context(mUIThread));
}

task<void> MainWindow::FrameLoop() {
  // If something goes wrong, crash immediately, instead of when closing the
  // app
  co_await this_task::fatal_on_uncaught_exception();

  const auto stop = mFrameLoopStopSource.get_token();
  constexpr auto NanosPerFrame
    = std::chrono::nanoseconds(static_cast<int64_t>(1e9) / FramesPerSecond);
  using HighResolutionTimerTick
    = std::chrono::duration<int64_t, std::ratio<1, 1'0'000'000>>;

  const wil::unique_event timer {CreateWaitableTimerExW(
    nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)};

  while (!stop.stop_requested()) {
    const auto start = std::chrono::steady_clock::now();
    const auto nextGoal = start + NanosPerFrame;

    co_await this->FrameTick(nextGoal);

    const auto wait = nextGoal - std::chrono::steady_clock::now();
    if (wait <= decltype(wait)::zero()) {
      TraceLoggingWrite(
        gTraceProvider, "MainWindow::FrameLoop()/FrameDurationExceeded");
      continue;
    }
    const LARGE_INTEGER waitTime {
      .QuadPart
      = -std::chrono::duration_cast<HighResolutionTimerTick>(wait).count()};
    if (!SetWaitableTimer(timer.get(), &waitTime, 0, nullptr, nullptr, 0)) {
      dprint.Error(
        "Failed to set frame timer: {}",
        winrt::hresult(HRESULT_FROM_WIN32(GetLastError())));
      co_return;
    }
    TraceLoggingWrite(
      gTraceProvider,
      "MainWindow::FrameLoop()/WaitStart",
      TraceLoggingValue(waitTime.QuadPart, "interval"));
    co_await OpenKneeboard::resume_on_signal(timer.get(), stop);
    TraceLoggingWrite(gTraceProvider, "MainWindow::FrameLoop()/WaitComplete");
  }
  co_return;
}

void MainWindow::CheckForElevatedConsumer() {
  if (IsElevated()) {
    return;
  }

  const auto pid = SHM::ActiveConsumers::Get().mElevatedConsumerProcessID;
  if (!pid) {
    return;
  }

  ShowWarningIfElevated(pid);
}

OpenKneeboard::fire_and_forget MainWindow::ShowWarningIfElevated(DWORD pid) {
  if (pid == mElevatedConsumerProcessID) {
    co_return;
  }

  co_await winrt::resume_background();

  std::filesystem::path path;
  {
    winrt::handle handle {
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid)};
    if (!handle) {
      co_return;
    }
    if (!IsElevated(handle.get())) {
      co_return;
    }

    wchar_t buf[MAX_PATH];
    auto size = static_cast<DWORD>(std::size(buf));
    if (!QueryFullProcessImageNameW(handle.get(), 0, buf, &size)) {
      co_return;
    }
    path = std::wstring_view {buf, size};
  }

  mElevatedConsumerProcessID = pid;

  co_await mUIThread;

  const auto message = std::format(
    _(L"'{}' (process {}) is running elevated; this WILL cause "
      L"problems.\n\nIt "
      L"is STRONGLY recommended that you do not run games elevated.\n\n"
      L"Running games as administrator is unsupported;\n"
      L"DO NOT ASK FOR HELP AND DO NOT REPORT ANY BUGS."),
    path.filename().wstring(),
    pid);

  ContentDialog dialog;
  dialog.XamlRoot(Navigation().XamlRoot());

  dialog.Title(box_value(to_hstring(_(L"Game is running as administrator"))));
  dialog.Content(box_value(message));
  dialog.PrimaryButtonText(_(L"OK"));
  dialog.DefaultButton(ContentDialogButton::Primary);

  dprint.Warning(
    "Showing game elevation warning dialog for PID {} ({})",
    pid,
    path.filename().string());
  co_await dialog.ShowAsync();
  dprint("Game elevation warning dialog closed.");
}

task<void> MainWindow::FrameTick(
  std::chrono::steady_clock::time_point nextFrameAt) {
  TraceLoggingActivity<gTraceProvider> activity;
  // Including the build number just to make sure it's in every trace
  TraceLoggingWriteStart(
    activity, "FrameTick", TraceLoggingValue(Version::Build, "BuildNumber"));
  // Some Microsoft components have a habit of re-entering the Windows message
  // loop from unreasonable places, e.g.:
  // - Webp Image Extensions re-enters the loop from
  //   IWICImagingFactory::CreateDecoder() and friends
  // - Windows.Data.Pdf.PdfDocument re-enters the loop from its' destructor
  const std::unique_lock recursiveFrameLock(mFrameInProgress, std::try_to_lock);
  if (!recursiveFrameLock) {
    co_return;
  }

  std::shared_lock kbLock(*mKneeboard, std::try_to_lock);
  if (!kbLock.owns_lock()) {
    TraceLoggingWriteStop(
      activity,
      "FrameTick",
      TraceLoggingValue("could not acquire kneeboard lock", "Result"));
    co_return;
  }
  TraceLoggingWriteTagged(activity, "Acquired shared kneeboard locked");

  this->CheckForElevatedConsumer();
  OPENKNEEBOARD_TraceLoggingScope("evFrameTimerPreEvent.emit()");
  mKneeboard->evFrameTimerPreEvent.Emit();
  TraceLoggingWriteTagged(activity, "Prepared to render");
  bool repainted = false;
  if (mKneeboard->IsRepaintNeeded()) {
    const std::unique_lock dxLock(*mDXR);
    TraceLoggingWriteTagged(activity, "DX locked");
    OPENKNEEBOARD_TraceLoggingCoro("Paint");
    if (auto ipc = mKneeboard->GetInterprocessRenderer()) {
      co_await ipc->RenderNow();
    }
    if (auto tab = Frame().Content().try_as<TabPage_WinRT>()) {
      co_await get_self<TabPage_Implementation>(tab)->PaintNow();
    }
    mKneeboard->Repainted();
    repainted = true;
  }

  TraceLoggingWriteStop(
    activity, "FrameTick", TraceLoggingValue(repainted, "Repainted"));
  {
    OPENKNEEBOARD_TraceLoggingScope("evFrameTimerPostEvent.emit()");
    mKneeboard->evFrameTimerPostEvent.Emit(
      repainted ? FramePostEventKind::WithRepaint
                : FramePostEventKind::WithoutRepaint);
  }
  kbLock.unlock();

  // Finish any pending UI stuff
  co_await wil::resume_foreground(this->DispatcherQueue());
  co_await mKneeboard->FlushOrderedEventQueue(nextFrameAt);
}

OpenKneeboard::fire_and_forget MainWindow::OnLoaded() {
  // WinUI3 gives us the spinning circle for a long time...
  SetCursor(LoadCursorW(NULL, IDC_ARROW));

  this->Show();
  SetWindowSubclass(
    mHwnd, &MainWindow::SubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));

  mWinEventHook.reset(SetWinEventHook(
    EVENT_SYSTEM_FOREGROUND,
    EVENT_SYSTEM_FOREGROUND,
    NULL,
    &MainWindow::WinEventProc,
    0,
    0,
    WINEVENT_OUTOFCONTEXT));
  mKneeboard->NotifyAppWindowIsForeground(GetForegroundWindow() == mHwnd);

  co_await this->WriteInstanceData();

  auto xamlRoot = this->Content().XamlRoot();
  mFrameLoop = this->FrameLoop();

  const auto updateResult
    = co_await CheckForUpdates(UpdateCheckType::Automatic, xamlRoot);
  if (updateResult == UpdateResult::InstallingUpdate) {
    co_return;
  }
  if (mKneeboard) {
    co_await InstallPlugin(mKneeboard, xamlRoot, GetCommandLineW());
  }

  co_await CheckAllDCSHooks(xamlRoot);
  co_await PromptForViewMode();
}

void MainWindow::WinEventProc(
  HWINEVENTHOOK,
  DWORD event,
  HWND hwnd,
  LONG /*idObject*/,
  LONG /*idChild*/,
  DWORD /*idEventThread*/,
  DWORD /*dwmsEventTime*/) {
  if (event != EVENT_SYSTEM_FOREGROUND) {
    return;
  }

  auto kneeboard = gKneeboard.lock();
  if (!kneeboard) {
    return;
  }
  kneeboard->NotifyAppWindowIsForeground(hwnd == gMainWindow);
}

task<void> MainWindow::PromptForViewMode() {
  auto weakThis = get_weak();

  auto viewSettings = mKneeboard->GetViewsSettings();
  if (viewSettings.mAppWindowMode != AppWindowViewMode::NoDecision) {
    co_return;
  }
  if (viewSettings.mViews.size() < 2) {
    co_return;
  }

  const auto result = co_await AppWindowViewModeDialog().ShowAsync();
  const auto mode = static_cast<AppWindowViewMode>(
    AppWindowViewModeDialog().SelectedMode(result));

  if (mode == viewSettings.mAppWindowMode) {
    co_return;
  }

  const auto self = weakThis.get();
  if (!self) {
    co_return;
  }

  viewSettings.mAppWindowMode = mode;
  co_await self->mKneeboard->SetViewsSettings(viewSettings);
}

task<void> MainWindow::WriteInstanceData() {
  const auto path = MainWindow::GetInstanceDataPath();
  const bool uncleanShutdown = std::filesystem::exists(path);

  auto f = std::ofstream(path, std::ios::binary | std::ios::trunc);
  f << std::format(
    "PID\t{}\n"
    "HWND\t{}\n"
    "Mailslot\t{}\n"
    "StartTime\t{}\n"
    "Elevated\t{}\n",
    GetCurrentProcessId(),
    reinterpret_cast<uint64_t>(mHwnd),
    to_utf8(APIEvent::GetMailslotPath()),
    std::chrono::system_clock::now(),
    IsElevated())
    << std::endl;

  if (!uncleanShutdown) {
    co_return;
  }

  if (IsElevated()) {
    dprint("Ignoring unclean shutdown because running as administrator");
    co_return;
  }

  if (IsDebuggerPresent()) {
    dprint("Ignoring unclean shutdown because a debugger is attached");
    co_return;
  }

  DWORD ignoreUncleanShutdowns = 0;
  DWORD dataSize = sizeof(ignoreUncleanShutdowns);
  RegGetValueW(
    HKEY_CURRENT_USER,
    RegistrySubKey,
    L"IgnoreUncleanShutdowns",
    RRF_RT_DWORD,
    nullptr,
    &ignoreUncleanShutdowns,
    &dataSize);

  if (ignoreUncleanShutdowns) {
    co_return;
  }

  ContentDialog dialog;
  dialog.XamlRoot(Navigation().XamlRoot());
  dialog.Title(box_value(to_hstring(_(L"OpenKneeboard did not exit cleanly"))));
  dialog.Content(
    box_value(to_hstring(_(L"Would you like to report a crash or freeze?"))));
  dialog.PrimaryButtonText(_(L"Yes"));
  dialog.SecondaryButtonText(_(L"Never ask me again"));
  dialog.CloseButtonText(_(L"No"));
  dialog.DefaultButton(ContentDialogButton::Primary);

  const auto result = co_await dialog.ShowAsync();

  if (result == ContentDialogResult::Primary) {
    co_await LaunchURI("https://go.openkneeboard.com/unclean-exit");
    co_return;
  }

  if (result != ContentDialogResult::Secondary) {
    co_return;
  }

  ignoreUncleanShutdowns = 1;
  RegSetKeyValueW(
    HKEY_CURRENT_USER,
    RegistrySubKey,
    L"IgnoreUncleanShutdowns",
    REG_DWORD,
    &ignoreUncleanShutdowns,
    sizeof(ignoreUncleanShutdowns));
}

MainWindow::~MainWindow() {
  dprint("{}", __FUNCTION__);
  gMainWindow = {};
}

OpenKneeboard::fire_and_forget MainWindow::UpdateProfileSwitcherVisibility() {
  co_await mUIThread;
  // As of Windows App SDK v1.1.4, changing the visibility and signalling the
  // bound property changed doesn't correctly update the navigation view, even
  // if UpdateLayout() is also called, so we need to manually add/remove the
  // item

  auto footerItems = Navigation().FooterMenuItems();
  auto first = footerItems.First().Current();

  std::wstring title(L"OpenKneeboard");

  const scope_exit setTitle([&title, this]() {
    if (IsElevated()) {
      title += L" [Administrator]";
    }

    if (!Version::IsStableRelease) {
      if (Version::IsTaggedVersion) {
        title += std::format(L" - PRERELEASE {}", to_hstring(Version::TagName));
      } else if (Version::IsGithubActionsBuild) {
        title += std::format(L" - UNRELEASED VERSION #GHA{}", Version::Build);
      } else {
        title += L" - LOCAL DEVELOPMENT BUILD";
      }
    }

    Title(title);
    AppTitle().Text(title);
  });

  const auto settings = mKneeboard->GetProfileSettings();
  if (!settings.mEnabled) {
    Navigation().PaneCustomContent({nullptr});
    co_return;
  }
  Navigation().PaneCustomContent(mProfileSwitcher);

  auto uiProfiles = ProfileSwitcherFlyout().Items();
  uiProfiles.Clear();
  for (const auto& profile: settings.GetSortedProfiles()) {
    ToggleMenuFlyoutItem item;
    auto wname = to_hstring(profile.mName);
    item.Text(wname);
    uiProfiles.Append(item);

    auto weakItem = make_weak(item);
    item.Click(
      [](auto self, auto item, auto profile) -> OpenKneeboard::fire_and_forget {
        auto settings = self->mKneeboard->GetProfileSettings();
        if (settings.mActiveProfile == profile.mGuid) {
          item.IsChecked(true);
          co_return;
        }

        self->mTabSwitchReason = TabSwitchReason::ProfileSwitched;

        settings.mActiveProfile = profile.mGuid;
        co_await self->mKneeboard->SetProfileSettings(settings);
      } | bind_refs_front(this, item)
        | bind_front(profile) | drop_winrt_event_args());

    if (profile.mGuid == settings.mActiveProfile) {
      item.IsChecked(true);
      ProfileSwitcherLabel().Text(wname);

      ToolTip tooltip;
      tooltip.Content(
        box_value(std::format(_(L"Switch profiles - current is '{}'"), wname)));
      ToolTipService::SetToolTip(ProfileSwitcher(), tooltip);
      title += std::format(L" - {}", wname);
    }
  }

  uiProfiles.Append(MenuFlyoutSeparator {});
  MenuFlyoutItem settingsItem;
  settingsItem.Text(_(L"Edit profiles..."));
  settingsItem.Click([this](const auto&, const auto&) {
    Frame().Navigate(xaml_typename<ProfilesPage>());
  });
  uiProfiles.Append(settingsItem);
}

void MainWindow::ResetKneeboardView() {
  OPENKNEEBOARD_TraceLoggingScope("MainWindow::ResetKneeboardView()");
  if (mKneeboard->GetAppWindowView() == mKneeboardView) {
    return;
  }
  for (const auto& event: mKneeboardViewEvents) {
    this->RemoveEventListener(event);
  }

  std::vector<winrt::guid> previousTabs;
  if (mKneeboardView) {
    previousTabs = mKneeboardView->GetTabIDs();
  }

  mKneeboardView = mKneeboard->GetAppWindowView();

  mKneeboardViewEvents = {
    AddEventListener(
      mKneeboardView->evBookmarksChangedEvent,
      std::bind_front(&MainWindow::OnTabsChanged, this)),
    AddEventListener(
      mKneeboardView->evCurrentTabChangedEvent,
      std::bind_front(&MainWindow::OnTabChanged, this)),
  };

  if (mKneeboardView->GetTabIDs() != previousTabs) {
    this->OnTabsChanged();
  } else {
    this->OnTabChanged();
  }
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

  mWindowPosition = windowRect;
}

OpenKneeboard::fire_and_forget MainWindow::Shutdown() {
  TraceLoggingWrite(gTraceProvider, "MainWindow::Shutdown()");

  auto self = get_strong();
  self->RemoveAllEventListeners();
  mWinEventHook.reset();
  // TODO: a lot of this should be moved to the Application class.
  dprint("Removing instance data...");
  try {
    std::filesystem::remove(MainWindow::GetInstanceDataPath());
  } catch (const std::filesystem::filesystem_error&) {
  }
  gShuttingDown = true;

  if (mWindowPosition) {
    dprint("Saving window position");
    auto settings = mKneeboard->GetAppSettings();
    settings.mWindowRect = *mWindowPosition;
    co_await mKneeboard->SetAppSettings(settings);
  }

  dprint("Releasing kneeboard resources tied to hwnd");
  co_await self->mKneeboard->ReleaseHwndResources();

  dprint("Removing window subclass");
  RemoveWindowSubclass(self->mHwnd, &MainWindow::SubclassProc, 0);

  dprint("Stopping frame loop...");
  self->mFrameLoopStopSource.request_stop();
  if (self->mFrameLoop) {
    co_await std::move(self->mFrameLoop).value();
  }

  dprint("Stopping event system...");
  {
    auto event = Win32::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!event) {
      fatal_with_hresult(event.error());
    }
    EventBase::Shutdown(event->get());
    co_await winrt::resume_on_signal(event->get());
  }
  dprint("Waiting for UI thread");

  co_await self->mUIThread;

  dprint("Cleaning up kneeboard");

  self->mKneeboardView = {};

  co_await mKneeboard->DisposeAsync();

  self->mKneeboard = {};
  self->mDXR = nullptr;

  TryEnqueue(self->DispatcherQueue(), []() -> task<void> {
    auto app = winrt::Microsoft::UI::Xaml::Application::Current().as<App>();
    co_await app.as<App>()->CleanupAndExitAsync();
  });
}

OpenKneeboard::fire_and_forget MainWindow::OnTabChanged() noexcept {
  co_await mUIThread;
  OPENKNEEBOARD_TraceLoggingCoro("MainWindow::OnTabChanged()");

  if (mTabSwitchReason != TabSwitchReason::InAppNavSelection) {
    // Don't automatically move away from "Profiles"
    if (
      Frame().CurrentSourcePageType().Name
      == xaml_typename<ProfilesPage>().Name) {
      co_return;
    }

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
  }

  if (mKneeboardView != mKneeboard->GetAppWindowView()) {
    OPENKNEEBOARD_BREAK;
  }
  const auto tab = mKneeboardView->GetCurrentTab().lock();
  if (!tab) {
    this->Frame().Navigate(
      xaml_typename<TabPage_WinRT>(),
      winrt::box_value(ITab::RuntimeID(nullptr).GetTemporaryValue()));
    co_return;
  }

  const auto id = tab->GetRuntimeID();

  if (mNavigationItems) {
    for (auto it: mNavigationItems) {
      auto item = it.try_as<Control>();
      if (!item) {
        continue;
      }
      auto tag = item.Tag();
      if (!tag) {
        continue;
      }
      if (NavigationTag::unbox(tag).mTabID == id) {
        Navigation().SelectedItem(item);
        break;
      }
    }
  }

  if (mTabSwitchReason == TabSwitchReason::Other) {
    this->Frame().Navigate(
      xaml_typename<TabPage_WinRT>(),
      winrt::box_value(id.GetTemporaryValue()),
      Microsoft::UI::Xaml::Media::Animation::
        SuppressNavigationTransitionInfo {});
  } else {
    this->Frame().Navigate(
      xaml_typename<TabPage_WinRT>(), winrt::box_value(id.GetTemporaryValue()));
  }
  mTabSwitchReason = TabSwitchReason::Other;
}

OpenKneeboard::fire_and_forget MainWindow::OnAPIEvent(APIEvent ev) {
  if (ev.name != APIEvent::EVT_OKB_EXECUTABLE_LAUNCHED) {
    co_return;
  }
  co_await mUIThread;

  {
    FLASHWINFO flash {
      .cbSize = sizeof(FLASHWINFO),
      .hwnd = mHwnd,
      .dwFlags = FLASHW_ALL,
      .uCount = 1,
    };
    winrt::check_bool(FlashWindowEx(&flash));
  }

  const std::wstring commandLine {winrt::to_hstring(ev.value)};
  co_await InstallPlugin(
    mKneeboard, Navigation().XamlRoot(), commandLine.c_str());
}

OpenKneeboard::fire_and_forget MainWindow::OnTabsChanged() {
  co_await mUIThread;
  OPENKNEEBOARD_TraceLoggingScope("MainWindow::OnTabsChanged()");

  for (auto&& token: mTabsEvents) {
    this->RemoveEventListener(token);
  }
  mTabsEvents.clear();
  for (auto&& tab: mKneeboard->GetTabsList()->GetTabs()) {
    if (!tab) {
      continue;
    }
    // Make tab renaming affect left nav instantly
    mTabsEvents.push_back(this->AddEventListener(
      tab->evSettingsChangedEvent,
      &MainWindow::OnTabSettingsChanged | bind_refs_front(this, tab)));
  }

  // In theory, we could directly mutate Navigation().MenuItems();
  // unfortunately, NavigationView contains a race condition, so
  // `MenuItems().Clear()` is unsafe.
  //
  // Work around this by using a property instead.

  TraceLoggingWrite(
    gTraceProvider, "MainWindow::OnTabsChanged()/EmittingPropertyChanged");
  this->mPropertyChangedEvent(
    *this,
    Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L"NavigationItems"));

  this->OnTabChanged();
}
OpenKneeboard::fire_and_forget MainWindow::OnTabSettingsChanged(
  const std::shared_ptr<ITab> tab) {
  if (!mNavigationItems) {
    this->mPropertyChangedEvent(
      *this,
      Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L"NavigationItems"));
    co_return;
  }

  for (auto&& it: mNavigationItems) {
    const auto item = it.try_as<muxc::NavigationViewItem>();
    if (!item) {
      continue;
    }
    const auto tag = NavigationTag::unbox(item.Tag());
    if (tag.mTabID != tab->GetRuntimeID()) {
      continue;
    }
    item.Content(box_value(to_hstring(tab->GetTitle())));
    co_return;
  }
}

winrt::Windows::Foundation::Collections::IVector<
  winrt::Windows::Foundation::IInspectable>
MainWindow::NavigationItems() noexcept {
  OPENKNEEBOARD_TraceLoggingScope("MainWindow::NavigationItems()");
  const std::shared_lock lock(*mKneeboard);
  TraceLoggingWrite(
    gTraceProvider, "OpenKneeboard::NavigationItems()/HaveLock");
  auto navItems = winrt::single_threaded_vector<IInspectable>();
  navItems.Clear();

  decltype(mKneeboardView->GetBookmarks()) bookmarks;
  if (mKneeboardView && mKneeboard->GetUISettings().mBookmarks.mEnabled) {
    bookmarks = mKneeboardView->GetBookmarks();
  }
  auto bookmark = bookmarks.begin();
  size_t bookmarkCount = 0;

  for (auto tab: mKneeboard->GetTabsList()->GetTabs()) {
    muxc::NavigationViewItem item;
    item.Content(box_value(to_hstring(tab->GetTitle())));
    item.Tag(NavigationTag {tab->GetRuntimeID()}.box());

    auto glyph = tab->GetGlyph();
    if (!glyph.empty()) {
      muxc::FontIcon icon;
      icon.Glyph(winrt::to_hstring(glyph));
      item.Icon(icon);
    }

    navItems.Append(item);

    for (auto tabID = tab->GetRuntimeID();
         bookmark != bookmarks.end() && bookmark->mTabID == tabID;
         bookmark++) {
      bookmarkCount++;
      const auto title = bookmark->mTitle.empty()
        ? winrt::hstring(std::format(L"#{}", bookmarkCount))
        : to_hstring(bookmark->mTitle);

      muxc::NavigationViewItem bookmarkItem;
      bookmarkItem.Content(box_value(title));
      bookmarkItem.Tag(NavigationTag {tabID, bookmark->mPageID}.box());

      muxc::MenuFlyoutItem renameItem;
      renameItem.Text(_(L"Rename bookmark"));
      muxc::FontIcon renameIcon;
      renameIcon.Glyph(L"\uE8AC");
      renameItem.Icon(renameIcon);
      renameItem.Click(
        [bookmark = *bookmark, title](const auto& self, const auto& tab) {
          self->RenameBookmark(tab, bookmark, title);
        }
        | drop_winrt_event_args() | bind_refs_front(this, tab));

      muxc::MenuFlyout contextFlyout;
      contextFlyout.Items().Append(renameItem);
      bookmarkItem.ContextFlyout(contextFlyout);

      item.MenuItems().Append(bookmarkItem);
    }

    item.IsExpanded(true);

    muxc::MenuFlyout contextFlyout;
    {
      muxc::MenuFlyoutItem renameItem;
      renameItem.Text(_(L"Rename tab"));

      muxc::FontIcon icon;
      icon.Glyph(L"\uE8AC");
      renameItem.Icon(icon);

      renameItem.Click(
        &MainWindow::RenameTab | drop_winrt_event_args()
        | bind_refs_front(this, tab));
      contextFlyout.Items().Append(renameItem);
    }
    item.ContextFlyout(contextFlyout);
  }
  mNavigationItems = navItems;
  this->OnTabChanged();
  return navItems;
}

OpenKneeboard::fire_and_forget MainWindow::RenameTab(
  std::shared_ptr<ITab> tab) {
  OpenKneeboardApp::RenameTabDialog dialog;
  dialog.XamlRoot(Navigation().XamlRoot());
  dialog.TabTitle(to_hstring(tab->GetTitle()));

  const auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  const auto newName = to_string(dialog.TabTitle());
  if (newName.empty()) {
    co_return;
  }
  tab->SetTitle(newName);
}

OpenKneeboard::fire_and_forget MainWindow::RenameBookmark(
  std::shared_ptr<ITab> tab,
  Bookmark bookmark,
  winrt::hstring title) {
  OpenKneeboardApp::RenameTabDialog dialog;
  dialog.XamlRoot(Navigation().XamlRoot());
  dialog.TabTitle(to_hstring(title));
  dialog.Prompt(_(L"What would you like to rename this bookmark to?"));

  const auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  const auto newName = to_string(dialog.TabTitle());
  if (newName.empty()) {
    co_return;
  }

  auto bookmarks = tab->GetBookmarks();
  for (auto it = bookmarks.begin(); it != bookmarks.end(); ++it) {
    if (*it != bookmark) {
      continue;
    }

    it->mTitle = newName;
    break;
  }
  tab->SetBookmarks(bookmarks);
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

  auto boxedTag = item.Tag();
  if (!boxedTag) {
    return;
  }

  auto tag = NavigationTag::unbox(boxedTag);

  const auto tabID = tag.mTabID;

  if (tag.mPageID) {
    mTabSwitchReason = TabSwitchReason::InAppNavSelection;

    mKneeboardView->GoToBookmark({
      tabID,
      *tag.mPageID,
    });
    return;
  }

  const auto tab = mKneeboardView->GetCurrentTabView()->GetRootTab().lock();
  if (!tab) {
    return;
  }
  if (tabID != tab->GetRuntimeID()) {
    mTabSwitchReason = TabSwitchReason::InAppNavSelection;
    mKneeboardView->SetCurrentTabByRuntimeID(tabID);
    return;
  }

  // Current tab == desired tab - but is that what we're actually showing?
  if (
    Frame().CurrentSourcePageType().Name
    == xaml_typename<TabPage_WinRT>().Name) {
    return;
  }

  // Nope, perhaps we're in 'Settings' instead
  mTabSwitchReason = TabSwitchReason::InAppNavSelection;
  Frame().Navigate(
    xaml_typename<TabPage_WinRT>(),
    winrt::box_value(tabID.GetTemporaryValue()));
}

void MainWindow::OnBackRequested(
  const IInspectable&,
  const NavigationViewBackRequestedEventArgs&) noexcept {
  Frame().GoBack();
}

OpenKneeboard::fire_and_forget MainWindow::LaunchOpenKneeboardURI(
  std::string_view uriStr) {
  auto uri = winrt::Windows::Foundation::Uri(winrt::to_hstring(uriStr));
  auto path = winrt::to_string(uri.Path());

  if (path.starts_with('/')) {
    path = {path.begin() + 1, path.end()};
  }

  co_await mUIThread;

  using namespace SpecialURIs::Paths;

  if (path == SettingsInput) {
    Frame().Navigate(xaml_typename<InputSettingsPage>());
    co_return;
  }
  if (path == SettingsTabs) {
    Frame().Navigate(xaml_typename<TabsSettingsPage>());
    co_return;
  }

  if (path == "TeachingTips/ProfileSwitcher") {
    ProfileSwitcherTeachingTip().Target(mProfileSwitcher);
    ProfileSwitcherTeachingTip().IsOpen(true);
    co_return;
  }
}

winrt::Windows::Foundation::IInspectable MainWindow::NavigationTag::box()
  const {
  auto json = nlohmann::json::object();
  json["tab"] = mTabID.GetTemporaryValue();
  if (mPageID) {
    json["page"] = mPageID->GetTemporaryValue();
  }
  return box_value(to_hstring(json.dump()));
}

MainWindow::NavigationTag MainWindow::NavigationTag::unbox(IInspectable value) {
  auto str = winrt::unbox_value<winrt::hstring>(value);
  auto json = nlohmann::json::parse(to_string(str));
  NavigationTag ret {
    .mTabID
    = ITab::RuntimeID::FromTemporaryValue(json.at("tab").get<uint64_t>()),
  };
  if (json.contains("page")) {
    ret.mPageID = PageID::FromTemporaryValue(json.at("page").get<uint64_t>());
  }
  return ret;
}

void MainWindow::Show() {
  int argc {};
  const wil::unique_hlocal_ptr<PWSTR[]> argv {
    CommandLineToArgvW(GetCommandLineW(), &argc)};

  bool minimized = false;

  const constexpr std::wstring_view minimizedFlag(L"--minimized");

  for (int i = 0; i < argc; ++i) {
    if (argv[i] == minimizedFlag) {
      minimized = true;
    }
  }

  // WinUI3: 'should' call `this->Activate()`;
  // ... but that doesn't let us do anything other than restore
  // normally
  //
  // Always use `ShowWindow()` instead of `->Activate()` so that it's obvious
  // if
  // `->Activate()` starts to be required.
  ShowWindow(mHwnd, minimized ? SW_SHOWMINNOACTIVE : SW_SHOWNORMAL);
}

std::filesystem::path MainWindow::GetInstanceDataPath() {
  return Filesystem::GetLocalAppDataDirectory() / "instance.txt";
}

LRESULT MainWindow::SubclassProc(
  HWND hWnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam,
  UINT_PTR /*uIdSubclass*/,
  DWORD_PTR dwRefData) {
  auto self = reinterpret_cast<MainWindow*>(dwRefData);
  switch (uMsg) {
    case WM_SIZE:
    case WM_MOVE:
      self->SaveWindowPosition();
      break;
    case WM_ENDSESSION:
      dprint("Processing WM_QUERYENDSESSION");
      // This won't complete as the window message loop is never re-entered,
      // but we want to do as much as we can, especially clearing the 'unsafe
      // shutdown' marker
      self->Shutdown();
      break;
    default:
      // Just the default behavior
      break;
  }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

}// namespace winrt::OpenKneeboardApp::implementation
