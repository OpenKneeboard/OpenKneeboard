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

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/DebugPrivileges.hpp>
#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/GetMainHWND.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/OpenXRMode.hpp>
#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>
#include <OpenKneeboard/WebView2PageSource.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/tracing.hpp>
#include <OpenKneeboard/version.hpp>

#include <Psapi.h>
#include <ShlObj.h>
#include <shellapi.h>

#include <wil/registry.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <set>

#include <Dbghelp.h>
#include <signal.h>
#include <zip.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Navigation;
using namespace OpenKneeboardApp::implementation;
using namespace OpenKneeboardApp;
using namespace OpenKneeboard;

#include <MddBootstrap.h>

#include <WindowsAppSDK-VersionInfo.h>

namespace OpenKneeboard {
/* PS > [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.App")
 * cc76597c-1041-5d57-c8ab-92cf9437104a
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.App",
  (0xcc76597c, 0x1041, 0x5d57, 0xc8, 0xab, 0x92, 0xcf, 0x94, 0x37, 0x10, 0x4a));
}// namespace OpenKneeboard

App::App() {
  InitializeComponent();
  UnhandledException(
    [](IInspectable const&, UnhandledExceptionEventArgs const& e) {
      fatal_with_hresult(e.Exception());
    });
}

task<void> App::CleanupAndExitAsync() {
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

OpenKneeboard::fire_and_forget App::OnLaunched(
  LaunchActivatedEventArgs) noexcept {
  DispatcherShutdownMode(
    winrt::Microsoft::UI::Xaml::DispatcherShutdownMode::OnExplicitShutdown);

  auto window = make<MainWindow>();
  mWindow = window;
  co_await winrt::get_self<implementation::MainWindow>(window)->Init();
}

static void MigrateBackups(const std::filesystem::path& backupsDirectory) {
  const auto oldBackupsDirectory
    = Filesystem::GetLocalAppDataDirectory() / "Backups";
  if (!std::filesystem::exists(oldBackupsDirectory)) {
    return;
  }

  // Is it a shortcut to the new backups directory?
  if (Filesystem::IsDirectoryShortcut(oldBackupsDirectory)) {
    return;
  }

  for (auto&& it:
       std::filesystem::recursive_directory_iterator(oldBackupsDirectory)) {
    if (!it.is_regular_file()) {
      continue;
    }
    const auto relative
      = std::filesystem::relative(it.path(), oldBackupsDirectory);
    std::filesystem::rename(it.path(), backupsDirectory / relative);
  }
  std::filesystem::remove_all(oldBackupsDirectory);
}

static void CreateBackupsShortcut(
  const std::filesystem::path& backupsDirectory) {
  const auto shortcutFrom = Filesystem::GetLocalAppDataDirectory() / "Backups";
  if (std::filesystem::exists(shortcutFrom)) {
    return;
  }

  Filesystem::CreateDirectoryShortcut(backupsDirectory, shortcutFrom);
}

static void BackupSettings() {
  const auto settingsPath = Filesystem::GetSettingsDirectory();
  if (std::filesystem::is_empty(settingsPath)) {
    return;
  }

  // Now we create backups outside of that so that people who manually delete
  // the entire `%LOCALAPPDATA%\OpenKneeboard` folder don't *accidentally*
  // delete the backups too
  const auto backupsDirectory
    = Filesystem::GetKnownFolderPath<FOLDERID_LocalAppData>()
    / "OpenKneeboard Backups";
  std::filesystem::create_directories(backupsDirectory);
  MigrateBackups(backupsDirectory);
  CreateBackupsShortcut(backupsDirectory);

  const auto lastVersion = wil::reg::try_get_value_string(
    HKEY_CURRENT_USER, Config::RegistrySubKey, L"AppVersionAtLastBackup");
  if (lastVersion == Version::ReleaseNameW) {
    return;
  }

  const auto now = std::chrono::zoned_time(
    std::chrono::current_zone(),
    std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now()));
  const auto backupFile = backupsDirectory
    / std::format("OpenKneeboard-Settings-{:%Y%m%dT%H%M}.zip", now);

  scope_success updateLastRun([backupFile]() {
    wil::reg::set_value_string(
      HKEY_CURRENT_USER,
      Config::RegistrySubKey,
      L"AppVersionAtLastBackup",
      Version::ReleaseNameW);
    dprint("🦺 Saved settings backup to `{}`", backupFile);
  });

  using unique_zip = std::unique_ptr<zip_t, decltype(&zip_close)>;
  int zipError {};
  unique_zip zip {
    zip_open(backupFile.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &zipError),
    &zip_close};

  for (auto&& it: std::filesystem::recursive_directory_iterator(settingsPath)) {
    if (it.is_directory()) {
      continue;
    }
    if (it.path().extension() != ".json") {
      continue;
    }
    const auto relative = std::filesystem::relative(it.path(), settingsPath);
    zip_file_add(
      zip.get(),
      relative.string().c_str(),
      zip_source_file(
        zip.get(), it.path().string().c_str(), 0, ZIP_LENGTH_TO_END),
      ZIP_FL_ENC_UTF_8);
  }
}

static void LogSystemInformation() {
  dprint("{} {}", ProjectReverseDomainA, Version::ReleaseName);
  dprint(L"Full path: {}", GetFullPathForCurrentExecutable());
  dprint(L"Command line: {}", GetCommandLineW());
  dprint("----------");
  dprint("  Elevated: {}", IsElevated());
  dprint("  Shell Elevated: {}", IsShellElevated());
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
        DWORD data {};
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
          dprint(
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
    dprint("  WebView2: v{}", webView2);
  } else {
    dprint("  WebView2: NOT FOUND");
  }

  CPINFOEXW codePageInfo;
  GetCPInfoExW(CP_ACP, 0, &codePageInfo);
  dprint(L"  Active code page: {}", codePageInfo.CodePageName);

  DWORD codePage {};
  GetLocaleInfoW(
    LOCALE_SYSTEM_DEFAULT,
    LOCALE_IDEFAULTCODEPAGE | LOCALE_RETURN_NUMBER,
    reinterpret_cast<LPWSTR>(&codePage),
    sizeof(codePage));
  GetCPInfoExW(codePage, 0, &codePageInfo);
  dprint(L"  System code page: {}", codePageInfo.CodePageName);
  GetLocaleInfoW(
    LOCALE_USER_DEFAULT,
    LOCALE_IDEFAULTCODEPAGE | LOCALE_RETURN_NUMBER,
    reinterpret_cast<LPWSTR>(&codePage),
    sizeof(codePage));
  dprint(L"  User code page: {}", codePageInfo.CodePageName);
  wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
  LCIDToLocaleName(LOCALE_SYSTEM_DEFAULT, localeName, sizeof(localeName), 0);
  dprint(L"  System locale: {}", localeName);
  LCIDToLocaleName(LOCALE_USER_DEFAULT, localeName, sizeof(localeName), 0);
  dprint(L"  User locale: {}", localeName);

  uint64_t totalMemoryKB {0};
  GetPhysicallyInstalledSystemMemory(&totalMemoryKB);
  dprint("  Total RAM: {}mb", totalMemoryKB / 1024);
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
          dprint("Found utilities path: {}", subdir);
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
    dprint.Error("failed to find utilities path");
  }
}

static void LogInstallationInformation() {
  {
    const auto dir = Filesystem::GetSettingsDirectory();
    const auto dirStr = dir.wstring();
    dprint(L"Settings directory: {}", dirStr);
    RegSetKeyValueW(
      HKEY_CURRENT_USER,
      Config::RegistrySubKey,
      L"SettingsPath",
      REG_SZ,
      dirStr.c_str(),
      static_cast<DWORD>((dirStr.size() + 1) * sizeof(wchar_t)));
  }

  auto binDir = Filesystem::GetRuntimeDirectory();
  dprint(L"Runtime directory: {}", binDir);

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
      dprint(
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
      dprint(
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
      dprint(L"Failed to read FileVersion for {}", path.filename().wstring());
      continue;
    }
    const auto versionStrLen = versionStrSize - 1;

    const auto modifiedAt = std::filesystem::last_write_time(path);

    dprint(
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
    Filesystem::GetKnownFolderPath<FOLDERID_SavedGames>();
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

  OpenKneeboard::divert_process_failure_to_fatal();
  const auto fullDumps
    = wil::reg::try_get_value_dword(
        HKEY_LOCAL_MACHINE, Config::RegistrySubKey, L"CreateFullDumps")
        .value_or(0);
  SetDumpType(fullDumps ? DumpType::FullDump : DumpType::MiniDump);

  if (RelaunchWithDesiredElevation(GetDesiredElevation(), showCommand)) {
    return 0;
  }

  // CreateMutex can set ERROR_ALREADY_EXISTS on success, so we need to
  // have a known-succeeding initial state.
  SetLastError(ERROR_SUCCESS);
  auto mutex
    = Win32::CreateMutex(nullptr, TRUE, OpenKneeboard::ProjectReverseDomainW);
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    // This can still be success
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
    if (SetForegroundWindow(*hwnd)) {
      APIEvent::Send({
        .name = APIEvent::EVT_OKB_EXECUTABLE_LAUNCHED,
        .value = winrt::to_string(GetCommandLineW()),
      });
    } else {
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
  } else if (mutex.has_value()) {
    gMutex = std::move(mutex).value();
  } else if (mutex.error()) {
    fatal(
      "Unexpected error creating mutex: {:#018x}",
      static_cast<uint32_t>(mutex.error()));
  }

  DPrintSettings::Set({
    .prefix = "OpenKneeboard-WinUI3",
  });

  winrt::init_apartment(winrt::apartment_type::single_threaded);
  SetThreadDescription(GetCurrentThread(), L"UI Thread");

  // Strong
  auto troubleshootingStore = TroubleshootingStore::Get();
  // Weak
  gTroubleshootingStore = troubleshootingStore;

  LogSystemInformation();
  LogInstallationInformation();
  SetRegistryValues();

  dprint("Cleaning up temporary directories...");
  Filesystem::CleanupTemporaryDirectories();

  Filesystem::MigrateSettingsDirectory();
  BackupSettings();

  for (auto&& dir:
       {Filesystem::GetLocalAppDataDirectory(),
        Filesystem::GetSettingsDirectory()}) {
    const auto warningFile = dir / "DO_NOT_PUT_YOUR_FILES_HERE-README.txt";
    if (std::filesystem::exists(warningFile)) {
      continue;
    }

    std::ofstream f(warningFile, std::ios::trunc | std::ios::binary);
    f << "Do not put any of your files here; this directory is for "
         "OpenKneeboard's internal use, and OpenKneeboard may delete any files "
         "you put here without warning.\n\n"
         "You might want to use the My Documents folder ("
      << Filesystem::GetKnownFolderPath<FOLDERID_Documents>().string()
      << ") or a new subfolder of your user folder ("
      << Filesystem::GetKnownFolderPath<FOLDERID_Profile>().string()
      << ") instead." << std::endl;
  }

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