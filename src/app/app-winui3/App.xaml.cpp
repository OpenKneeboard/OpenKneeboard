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

#include "Globals.h"
#include "MainWindow.xaml.h"

#include <OpenKneeboard/Elevation.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/GetMainHWND.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/TroubleshootingStore.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>

#include <shims/filesystem>

#include <chrono>
#include <exception>
#include <set>

#include <Dbghelp.h>
#include <Psapi.h>
#include <ShlObj.h>
#include <appmodel.h>
#include <shellapi.h>
#include <signal.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Navigation;
using namespace OpenKneeboardApp::implementation;
using namespace OpenKneeboardApp;
using namespace OpenKneeboard;

#include <mddbootstrap.h>

#include <WindowsAppSDK-VersionInfo.h>

static std::filesystem::path gDumpDirectory;
static bool gDumped = false;

namespace OpenKneeboard {
/* PS > [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.App")
 * cc76597c-1041-5d57-c8ab-92cf9437104a
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.App",
  (0xcc76597c, 0x1041, 0x5d57, 0xc8, 0xab, 0x92, 0xcf, 0x94, 0x37, 0x10, 0x4a));
}// namespace OpenKneeboard

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
  winrt::file_handle dumpFile {CreateFileW(
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
}

static void LogSystemInformation() {
  dprintf("{} {}", ProjectNameA, Version::ReleaseName);
  dprint("----------");
  dprintf("  Elevated: {}", IsElevated());
  dprintf("  Shell Elevated: {}", IsShellElevated());

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

static void LogInstallationInformation() {
  const auto dir = Filesystem::GetRuntimeDirectory();
  dprintf(L"Installation directory: {}", dir.wstring());
  for (const auto& entry: std::filesystem::directory_iterator(dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto path = entry.path();
    const auto ext = path.extension();
    if (ext != L".dll" && ext != L".exe") {
      continue;
    }
    if (!path.filename().wstring().starts_with(L"OpenKneeboard")) {
      continue;
    }

    const auto pathStr = path.wstring();
    DWORD ignored {};
    const auto versionSize = GetFileVersionInfoSizeExW(
      FILE_VER_GET_NEUTRAL, pathStr.c_str(), &ignored);
    if (versionSize == 0) {
      dprintf(
        L"Failed to get version info size for {}: {}",
        path.filename().wstring(),
        GetLastError());
      continue;
    }

    std::vector<char*> versionBuf(versionSize);
    if (!GetFileVersionInfoExW(
          FILE_VER_GET_NEUTRAL | FILE_VER_GET_PREFETCHED,
          pathStr.c_str(),
          NULL,
          static_cast<DWORD>(versionBuf.size()),
          versionBuf.data())) {
      dprintf(
        L"Failed to get version info for {}: {}",
        path.filename().wstring(),
        GetLastError());
      continue;
    }

    wchar_t* versionStr {nullptr};
    UINT versionStrSize {};
    if (!VerQueryValueW(
          versionBuf.data(),
          L"\\StringFileInfo\\040904E4\\FileVersion",
          reinterpret_cast<void**>(&versionStr),
          &versionStrSize)) {
      dprintf(L"Failed to read FileVersion for {}", path.filename().wstring());
      continue;
    }
    const auto versionStrLen = versionStrSize - 1;

    const auto modifiedAt = std::filesystem::last_write_time(path);

    dprintf(
      L"{:<48s} v{}\t{}",
      path.filename().wstring(),
      std::wstring_view {versionStr, versionStrLen},
      modifiedAt);
  }
  dprint("----------");
}

/** Microsoft have published updates to the Windows App SDK which lower the
 * version number.
 *
 * This makes MSI very very unhappy, so fix it up.
 */
static bool RepairInstallation() {
  DWORD repair {};
  DWORD dataSize = sizeof(repair);
  const auto ret = RegGetValueW(
    HKEY_LOCAL_MACHINE,
    RegistrySubKey,
    L"RepairOnNextRun",
    RRF_RT_REG_DWORD,
    nullptr,
    &repair,
    &dataSize);
  if (ret != ERROR_SUCCESS) {
    OutputDebugStringA(
      std::format(
        "Failed to read registry value to tell if we need repair: {}",
        static_cast<int64_t>(ret))
        .c_str());
    return false;
  }

  if (!repair) {
    OutputDebugStringA("Repair not needed");
    return false;
  }

  OutputDebugStringA("Repair needed, spawning repair subprocess");
  const auto path = std::format(
    L"{}\\{}",
    Filesystem::GetRuntimeDirectory(),
    RuntimeFiles::REPAIR_INSTALLATION_HELPER);
  SHELLEXECUTEINFOW sei {
    .cbSize = sizeof(SHELLEXECUTEINFOW),
    .fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS,
    .lpVerb = L"runas",
    .lpFile = path.c_str(),
    .lpParameters = path.c_str(),
  };
  if (!ShellExecuteExW(&sei)) {
    OutputDebugStringA(
      std::format("Repair ShellExecuteEx failed: {}", GetLastError()).c_str());
    return false;
  }
  OutputDebugStringA("Waiting for repair process...");
  WaitForSingleObject(sei.hProcess, INFINITE);
  CloseHandle(sei.hProcess);
  OutputDebugStringA("Repair complete.");
  return true;
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int showCommand) {
  RepairInstallation();

  TraceLoggingRegister(gTraceProvider);
  const scope_guard unregisterTraceProvider(
    []() { TraceLoggingUnregister(gTraceProvider); });

  wchar_t* savedGamesBuffer = nullptr;
  winrt::check_hresult(
    SHGetKnownFolderPath(FOLDERID_SavedGames, NULL, NULL, &savedGamesBuffer));
  gDumpDirectory = std::filesystem::path {std::wstring_view {savedGamesBuffer}}
    / "OpenKneeboard";
  std::filesystem::create_directories(gDumpDirectory);
  SetUnhandledExceptionFilter(&OnUnhandledException);
  set_terminate(&OnTerminate);

  if (RelaunchWithDesiredElevation(GetDesiredElevation(), showCommand)) {
    return 0;
  }

  gMutex
    = winrt::handle {CreateMutexW(nullptr, TRUE, OpenKneeboard::ProjectNameW)};
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

  gTroubleshootingStore = TroubleshootingStore::Get();
  LogSystemInformation();
  LogInstallationInformation();

  dprint("Cleaning up temporary directories...");
  Filesystem::CleanupTemporaryDirectories();

  dprint("Starting Xaml application");
  dprint("----------");

  ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
    ::winrt::make<::winrt::OpenKneeboardApp::implementation::App>();
  });

  return 0;
}
