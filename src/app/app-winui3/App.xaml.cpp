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
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/version.h>
#include <Psapi.h>
#include <ShlObj.h>
#include <appmodel.h>
#include <fmt/chrono.h>
#include <signal.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <set>

#include "CheckDCSHooks.h"
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

#pragma comment(lib, "Dbghelp.lib")

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

  auto fileName = fmt::format(
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
    fakeExceptionPointers = { &exceptionRecord, &exceptionContext };
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

static void InstallOpenXRLayerInUnsandboxedSubprocess() {
  auto layerPath = RuntimeFiles::GetDirectory().wstring();
  auto exePath = (RuntimeFiles::GetDirectory()
                  / RuntimeFiles::OPENXR_REGISTER_LAYER_HELPER)
                   .wstring();

  auto commandLine = fmt::format(L"\"{}\" \"{}\"", exePath, layerPath);
  commandLine.reserve(32767);

  STARTUPINFOEXW startupInfo {};
  startupInfo.StartupInfo.cb = sizeof(startupInfo);
  SIZE_T attributeListSize;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);
  startupInfo.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
    HeapAlloc(GetProcessHeap(), 0, attributeListSize));
  DWORD policy = PROCESS_CREATION_DESKTOP_APP_BREAKAWAY_ENABLE_PROCESS_TREE;
  UpdateProcThreadAttribute(
    startupInfo.lpAttributeList,
    0,
    PROC_THREAD_ATTRIBUTE_DESKTOP_APP_POLICY,
    &policy,
    sizeof(policy),
    nullptr,
    nullptr);

  PROCESS_INFORMATION procInfo {};

  CreateProcessW(
    exePath.c_str(),
    commandLine.data(),
    nullptr,
    nullptr,
    FALSE,
    0,
    nullptr,
    nullptr,
    reinterpret_cast<LPSTARTUPINFOW>(&startupInfo),
    &procInfo);
}

void App::OnLaunched(LaunchActivatedEventArgs const&) noexcept {
  window = make<MainWindow>();

  window.Content().as<winrt::Microsoft::UI::Xaml::FrameworkElement>().Loaded(
    [=](auto&, auto&) noexcept -> winrt::fire_and_forget {
      co_await CheckAllDCSHooks(window.Content().XamlRoot());
    });
  InstallOpenXRLayerInUnsandboxedSubprocess();
  window.Activate();
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

  DPrintSettings::Set({
    .prefix = "OpenKneeboard-WinUI3",
  });

  winrt::handle mutex {
    CreateMutexW(nullptr, TRUE, OpenKneeboard::ProjectNameW)};
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    auto name = fmt::format(L"Local\\{}.hwnd", OpenKneeboard::ProjectNameW);
    winrt::handle hwndFile {
      OpenFileMapping(PAGE_READWRITE, FALSE, name.c_str())};
    if (!hwndFile) {
      return 1;
    }
    void* mapping
      = MapViewOfFile(hwndFile.get(), FILE_MAP_READ, 0, 0, sizeof(HWND));
    auto hwnd = *reinterpret_cast<HWND*>(mapping);
    UnmapViewOfFile(mapping);
    ShowWindow(hwnd, SW_SHOWNORMAL) && SetForegroundWindow(hwnd);
    return 0;
  }

  UINT32 packageNameLen {0};
  const bool isPackaged = GetCurrentPackageFullName(&packageNameLen, nullptr)
    == ERROR_INSUFFICIENT_BUFFER;
  if (!isPackaged) {
    PACKAGE_VERSION minimumVersion {WINDOWSAPPSDK_RUNTIME_VERSION_UINT64};
    winrt::check_hresult(MddBootstrapInitialize(
      WINDOWSAPPSDK_RELEASE_MAJORMINOR,
      WINDOWSAPPSDK_RELEASE_VERSION_TAG_W,
      minimumVersion));
  }

  {
    void(WINAPI * pfnXamlCheckProcessRequirements)();
    auto module = ::LoadLibrary(L"Microsoft.ui.xaml.dll");
    if (module) {
      pfnXamlCheckProcessRequirements
        = reinterpret_cast<decltype(pfnXamlCheckProcessRequirements)>(
          GetProcAddress(module, "XamlCheckProcessRequirements"));
      if (pfnXamlCheckProcessRequirements) {
        (*pfnXamlCheckProcessRequirements)();
      }

      ::FreeLibrary(module);
    }
  }

  winrt::init_apartment(winrt::apartment_type::single_threaded);
  ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
    ::winrt::make<::winrt::OpenKneeboardApp::implementation::App>();
  });

  if (!isPackaged) {
    MddBootstrapShutdown();
  }

  return 0;
}
