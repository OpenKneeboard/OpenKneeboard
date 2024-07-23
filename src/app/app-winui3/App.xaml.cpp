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

#include <OpenKneeboard/config.h>

#include "Globals.h"
#include "MainWindow.xaml.h"

#include <OpenKneeboard/DebugPrivileges.h>
#include <OpenKneeboard/Elevation.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/GetMainHWND.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/ProcessShutdownBlock.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/TroubleshootingStore.h>
#include <OpenKneeboard/WebView2PageSource.h>
#include <OpenKneeboard/Win32.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_exit.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>

#include <shims/filesystem>

#include <wil/registry.h>

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
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Navigation;
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
  const auto dumpFile = Win32::CreateFileW(
    filePath.c_str(),
    GENERIC_READ | GENERIC_WRITE,
    0,
    nullptr,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
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

winrt::fire_and_forget App::CleanupAndExitAsync() {
  auto keepAlive = get_strong();
  winrt::apartment_context uiThread;
  dprint("Starting app shutdown");
  winrt::handle event {CreateEventW(nullptr, TRUE, FALSE, nullptr)};
  ProcessShutdownBlock::SetEventOnCompletion(event.get());

  mWindow = {nullptr};

  dprint("Waiting for cleanup");

  if (!co_await winrt::resume_on_signal(event.get(), std::chrono::seconds(1))) {
    dprint("Failed to cleanup after 1 second, quitting anyway.");
    ProcessShutdownBlock::DumpActiveBlocks();
  }

  dprint("Exiting app");

  co_await uiThread;

  /* TODO (Windows App SDK v1.5?): This should be implied by Exit(),
   but probalby broken by the WM_DESTROY hook; this should be replaced by
   DispatcherShutdownMode when available */
  ::PostQuitMessage(0);
  TraceLoggingWrite(gTraceProvider, "PostQuitMessage()");
}

void App::OnLaunched(LaunchActivatedEventArgs const&) noexcept {
  DispatcherShutdownMode(
    winrt::Microsoft::UI::Xaml::DispatcherShutdownMode::OnExplicitShutdown);

  mWindow = make<MainWindow>();
}

static void LogSystemInformation() {
  dprintf("{} {}", ProjectReverseDomainA, Version::ReleaseName);
  dprintf(L"Full path: {}", GetFullPathForCurrentExecutable());
  dprint("----------");
  dprintf("  Elevated: {}", IsElevated());
  dprintf("  Shell Elevated: {}", IsShellElevated());
  // Log UAC settings because lower values aren't just "do not prompt" - they
  // will automatically run some things as administrator that otherwise would
  // be ran as a normal user. This causes problems.
  {
    wil::unique_hkey hkey;
    if (
      RegOpenKeyW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        hkey.put())
      == ERROR_SUCCESS) {
      for (const auto& name:
           {L"EnableLUA",
            L"PromptOnSecureDesktop",
            L"ConsentPromptBehaviorAdmin"}) {
        DWORD data;
        DWORD dataSize {sizeof(data)};
        if (
          RegGetValueW(
            hkey.get(),
            nullptr,
            name,
            RRF_RT_DWORD | RRF_ZEROONFAILURE,
            nullptr,
            &data,
            &dataSize)
          == ERROR_SUCCESS) {
          dprintf(
            L"  UAC {}: {:#010x} ({})",
            name,
            static_cast<uint32_t>(data),
            data);
        }
      }
    } else {
      dprint("  Failed to read UAC settings.");
    }
  }

  if (WebView2PageSource::IsAvailable()) {
    const auto webView2 = WebView2PageSource::GetVersion();
    dprintf("  WebView2: v{}", webView2);
  } else {
    dprint("  WebView2: NOT FOUND");
  }

  CPINFOEXW codePageInfo;
  GetCPInfoExW(CP_ACP, 0, &codePageInfo);
  dprintf(L"  Active code page: {}", codePageInfo.CodePageName);

  DWORD codePage {};
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

static void SetRegistryValues() {
  auto savePath = [](auto name, auto path) {
    const auto pathStr = path.wstring();
    RegSetKeyValueW(
      HKEY_CURRENT_USER,
      Config::RegistrySubKey,
      name,
      REG_SZ,
      pathStr.c_str(),
      static_cast<DWORD>((pathStr.size() + 1) * sizeof(wchar_t)));
  };

  const auto binPath = Filesystem::GetRuntimeDirectory();
  savePath(L"InstallationBinPath", binPath);

  auto utilitiesPath = binPath.parent_path() / "utilities";
  if (!std::filesystem::exists(utilitiesPath)) {
    if constexpr (!OpenKneeboard::Version::IsGithubActionsBuild) {
      utilitiesPath = binPath;
      while (utilitiesPath.has_parent_path()) {
        const auto parent = utilitiesPath.parent_path();
        const auto subdir = parent / "utilities" / Config::BuildType;
        if (std::filesystem::exists(subdir)) {
          dprintf("Found utilities path: {}", subdir);
          utilitiesPath = subdir;
          break;
        }
        utilitiesPath = parent;
      }
    }
  }
  if (std::filesystem::exists(utilitiesPath)) {
    savePath(
      L"InstallationUtilitiesPath", std::filesystem::canonical(utilitiesPath));
  } else {
    dprint("WARNING: failed to find utilities path");
  }
}

static void LogInstallationInformation() {
  {
    const auto dir = Filesystem::GetSettingsDirectory();
    const auto dirStr = dir.wstring();
    dprintf(L"Settings directory: {}", dirStr);
    RegSetKeyValueW(
      HKEY_CURRENT_USER,
      Config::RegistrySubKey,
      L"SettingsPath",
      REG_SZ,
      dirStr.c_str(),
      static_cast<DWORD>((dirStr.size() + 1) * sizeof(wchar_t)));
  }

  auto binDir = Filesystem::GetRuntimeDirectory();
  dprintf(L"Runtime directory: {}", binDir);

  for (const auto& entry: std::filesystem::directory_iterator(binDir)) {
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

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int showCommand) {
  TraceLoggingRegister(gTraceProvider);
  const scope_exit unregisterTraceProvider(
    []() { TraceLoggingUnregister(gTraceProvider); });

  try {
    gDumpDirectory
      = Filesystem::GetKnownFolderPath<FOLDERID_SavedGames>() / "OpenKneeboard";
  } catch (const winrt::hresult_error& error) {
    const auto message = std::format(
      _(L"Windows was unable to find your 'Saved Games' folder; "
        L"OpenKneeboard "
        L"is unable to start.\n\nSHGetKnownFolderPath() failed: {:#08x} - "
        L"{}"),
      static_cast<const uint32_t>(error.code().value),
      std::wstring_view {error.message()});
    MessageBoxW(
      NULL,
      message.c_str(),
      _(L"Windows Configuration Error"),
      MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
    return 1;
  }
  std::filesystem::create_directories(gDumpDirectory);
  SetUnhandledExceptionFilter(&OnUnhandledException);
  set_terminate(&OnTerminate);

  if (RelaunchWithDesiredElevation(GetDesiredElevation(), showCommand)) {
    return 0;
  }

  gMutex
    = Win32::CreateMutexW(nullptr, TRUE, OpenKneeboard::ProjectReverseDomainW);
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

  // Strong
  auto troubleshootingStore = TroubleshootingStore::Get();
  // Weak
  gTroubleshootingStore = troubleshootingStore;

  LogSystemInformation();
  LogInstallationInformation();
  SetRegistryValues();

  dprint("Cleaning up temporary directories...");
  Filesystem::CleanupTemporaryDirectories();

  DebugPrivileges privileges;

  dprint("Starting Xaml application");
  dprint("----------");

  ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
    ::winrt::make<::winrt::OpenKneeboardApp::implementation::App>();
  });

  TraceLoggingWrite(gTraceProvider, "ApplicationExit");

  if (gDXResources.use_count() != 1) {
    dprint("----- POTENTIAL LEAK -----");
    gDXResources.dump_refs("gDXResources");
    OPENKNEEBOARD_BREAK;
  }
  gDXResources = nullptr;

  gTroubleshootingStore = {};

  return 0;
}
