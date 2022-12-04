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
#include "App.xaml.h"
// clang-format on

#include <Dbghelp.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/GetMainHWND.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/TroubleshootingStore.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/version.h>
#include <Psapi.h>
#include <ShlObj.h>
#include <appmodel.h>
#include <signal.h>

#include <chrono>
#include <exception>
#include <set>
#include <shims/filesystem>

#include "CheckDCSHooks.h"
#include "CheckForUpdates.h"
#include "Globals.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Navigation;
using namespace OpenKneeboardApp::implementation;
using namespace OpenKneeboardApp;
using namespace OpenKneeboard;

#include <WindowsAppSDK-VersionInfo.h>
#include <mddbootstrap.h>

static std::filesystem::path gDumpDirectory;
static bool gDumped = false;

static void CreateDump(LPEXCEPTION_POINTERS exceptionPointers) {
  if (gDumped) {
    return;
  }
  gDumped = true;
#ifdef DEBUG
  if (IsDebuggerPresent()) {
    __debugbreak();
  }
#endif
  const auto processId = GetCurrentProcessId();

  auto fileName = std::format(
    L"OpenKneeboard-{:%Y%m%dT%H%M%S}-{}.{}.{}.{}-{}.dmp",
    std::chrono::system_clock::now(),
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build,
    processId);
  auto filePath = (gDumpDirectory / fileName).wstring();
  winrt::handle dumpFile {CreateFileW(
    filePath.c_str(),
    GENERIC_READ | GENERIC_WRITE,
    0,
    nullptr,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL)};
  MINIDUMP_EXCEPTION_INFORMATION exceptionInfo {
    .ThreadId = GetCurrentThreadId(),
    .ExceptionPointers = exceptionPointers,
    .ClientPointers /* exception in debugger target */ = false,
  };

  EXCEPTION_RECORD exceptionRecord {};
  CONTEXT exceptionContext {};
  EXCEPTION_POINTERS fakeExceptionPointers;
  if (!exceptionPointers) {
    ::RtlCaptureContext(&exceptionContext);
    exceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
    fakeExceptionPointers = {&exceptionRecord, &exceptionContext};
    exceptionInfo.ExceptionPointers = &fakeExceptionPointers;
  }

  MiniDumpWriteDump(
    GetCurrentProcess(),
    processId,
    dumpFile.get(),
    static_cast<MINIDUMP_TYPE>(
      MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithProcessThreadData
      | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo),
    &exceptionInfo,
    /* user stream = */ nullptr,
    /* callback = */ nullptr);
}

LONG __callback WINAPI
OnUnhandledException(LPEXCEPTION_POINTERS exceptionPointers) {
  CreateDump(exceptionPointers);
  return EXCEPTION_EXECUTE_HANDLER;
}

static void OnTerminate() {
  CreateDump(nullptr);
  abort();
}

App::App() {
  InitializeComponent();
#ifdef DEBUG
  UnhandledException(
    [this](IInspectable const&, UnhandledExceptionEventArgs const& e) {
      if (IsDebuggerPresent()) {
        auto errorMessage = e.Message();
        __debugbreak();
      }
      throw;
    });
#endif
}

void App::OnLaunched(LaunchActivatedEventArgs const&) noexcept {
  window = make<MainWindow>();

  window.Content().as<winrt::Microsoft::UI::Xaml::FrameworkElement>().Loaded(
    [=](auto&, auto&) noexcept -> winrt::fire_and_forget {
      auto xamlRoot = window.Content().XamlRoot();
      co_await CheckForUpdates(UpdateCheckType::Automatic, xamlRoot);
      if (gKneeboard) {
        gKneeboard->GetGamesList()->StartInjector();
      }
      co_await CheckAllDCSHooks(xamlRoot);
    });
  window.Activate();
}

static void LogSystemInformation() {
  dprintf("{} {}", ProjectNameA, Version::ReleaseName);
  dprint("----------");

  CPINFOEXW codePageInfo;
  GetCPInfoExW(CP_ACP, 0, &codePageInfo);
  dprintf(L"  Active code page: {}", codePageInfo.CodePageName);

  DWORD codePage;
  GetLocaleInfoW(
    LOCALE_SYSTEM_DEFAULT,
    LOCALE_IDEFAULTCODEPAGE | LOCALE_RETURN_NUMBER,
    reinterpret_cast<LPWSTR>(&codePage),
    sizeof(codePage));
  GetCPInfoExW(codePage, 0, &codePageInfo);
  dprintf(L"  System code page: {}", codePageInfo.CodePageName);
  GetLocaleInfoW(
    LOCALE_USER_DEFAULT,
    LOCALE_IDEFAULTCODEPAGE | LOCALE_RETURN_NUMBER,
    reinterpret_cast<LPWSTR>(&codePage),
    sizeof(codePage));
  dprintf(L"  User code page: {}", codePageInfo.CodePageName);
  wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
  LCIDToLocaleName(LOCALE_SYSTEM_DEFAULT, localeName, sizeof(localeName), 0);
  dprintf(L"  System locale: {}", localeName);
  LCIDToLocaleName(LOCALE_USER_DEFAULT, localeName, sizeof(localeName), 0);
  dprintf(L"  User locale: {}", localeName);

  uint64_t totalMemoryKB {0};
  GetPhysicallyInstalledSystemMemory(&totalMemoryKB);
  dprintf("  Total RAM: {}mb", totalMemoryKB / 1024);
  dprint("----------");
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  wchar_t* savedGamesBuffer = nullptr;
  winrt::check_hresult(
    SHGetKnownFolderPath(FOLDERID_SavedGames, NULL, NULL, &savedGamesBuffer));
  gDumpDirectory = std::filesystem::path {std::wstring_view {savedGamesBuffer}}
    / "OpenKneeboard";
  std::filesystem::create_directories(gDumpDirectory);
  SetUnhandledExceptionFilter(&OnUnhandledException);
  set_terminate(&OnTerminate);

  winrt::handle mutex {
    CreateMutexW(nullptr, TRUE, OpenKneeboard::ProjectNameW)};
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    const auto hwnd = GetMainHWND();
    if (!hwnd) {
      MessageBoxW(
        NULL,
        _(L"OpenKneeboard is already running, but can't find the existing "
          L"window to switch to it.\n\n"
          L"Switch to it with Alt-Tab or the Windows "
          L"task bar, or kill it with Task Manager, then try again."),
        _(L"OpenKneeboard"),
        MB_OK | MB_ICONERROR);
      return 0;
    }
    ShowWindow(*hwnd, SW_SHOWNORMAL);
    if (!SetForegroundWindow(*hwnd)) {
      // error codes are not set, so no details :(
      MessageBoxW(
        NULL,
        _(L"OpenKneeboard is already running, but unable to switch to the "
          L"existing window .\n\n"
          L"Switch to it with Alt-Tab or the Windows "
          L"task bar, or kill it with Task Manager, then try again."),
        _(L"OpenKneeboard"),
        MB_OK | MB_ICONERROR);
    }
    return 0;
  }

  DPrintSettings::Set({
    .prefix = "OpenKneeboard-WinUI3",
  });

  winrt::init_apartment(winrt::apartment_type::single_threaded);

  auto troubleshootingStore = TroubleshootingStore::Get();
  LogSystemInformation();

  dprint("Cleaning up temporary directories...");
  Filesystem::CleanupTemporaryDirectories();

  dprint("Starting Xaml application");

  ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
    ::winrt::make<::winrt::OpenKneeboardApp::implementation::App>();
  });

  return 0;
}
