// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "App.xaml.h"
// clang-format on

#include "Globals.h"
#include "MainWindow.xaml.h"

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/ChromiumApp.hpp>
#include <OpenKneeboard/ChromiumWorker.hpp>
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
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/bitflags.hpp>
#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/tracing.hpp>
#include <OpenKneeboard/version.hpp>

#include <Psapi.h>
#include <ShlObj.h>
#include <shellapi.h>

#include <winrt/Windows.System.Profile.h>

#include <wil/registry.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <set>

#include <Dbghelp.h>
#include <MddBootstrap.h>
#include <signal.h>
#include <zip.h>

#include <WindowsAppSDK-VersionInfo.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Navigation;
using namespace OpenKneeboardApp::implementation;
using namespace OpenKneeboardApp;
using namespace OpenKneeboard;

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

  auto propertyNames = winrt::single_threaded_vector<winrt::hstring>({
    L"DeviceFamily",
    L"FlightRing",
    L"OSVersionFull",
  });
  auto properties = co_await winrt::Windows::System::Profile::AnalyticsInfo::
    GetSystemPropertiesAsync(propertyNames);
  dprint("----------");
  for (auto&& [name, value]: properties) {
    dprint(
      L"WinRT analytics {}: {}",
      std::wstring_view {name},
      std::wstring_view {value});
  }
  dprint(
    L"WinRT analytics DeviceForm: {}",
    std::wstring_view {
      winrt::Windows::System::Profile::AnalyticsInfo::DeviceForm()});
  dprint("----------");

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

enum class DamagingEnvironmentFlags : uint8_t {
  None = 0,
  Fatal = (1 << 0),
  ElevationRelated = (1 << 1),
  IsElevated = (1 << 2) | ElevationRelated | Fatal,
  UacIsDisabled = (1 << 3) | ElevationRelated | Fatal,
  UacWasPreviouslyDisabled = (1 << 4) | ElevationRelated,
  OlderThanWin10 = (1 << 5),
  Wine = (1 << 6),
};

constexpr bool supports_bitflags(DamagingEnvironmentFlags) {
  return true;
}

static void ShowDamagingEnvironmentError(const DamagingEnvironmentFlags flags) {
  using enum DamagingEnvironmentFlags;
  if ((flags & Wine) == Wine) {
    MessageBoxW(
      nullptr,
      L"Wine is detected; this is unsupported and problems should be expected. "
      L"Wine compatibility issues are not considered bugs.",
      L"OpenKneeboard",
      MB_OK | MB_ICONWARNING);
    return;
  }

  std::wstring_view elevationProblem;
  if ((flags & IsElevated) == IsElevated) {
    elevationProblem = _(L"OpenKneeboard is running elevated");
  } else if ((flags & UacIsDisabled) == UacIsDisabled) {
    elevationProblem = _(L"User Account Control (UAC) is disabled");
  } else if ((flags & UacWasPreviouslyDisabled) == UacWasPreviouslyDisabled) {
    MessageBoxW(
      nullptr,
      _(L"User Account Control (UAC) was previously disabled on this "
        L"system.\n\n"
        L"This can cause problems with your VR drivers, tablet drivers, games, "
        L"OpenKneeboard, and other software that can only be fixed by "
        L"reinstalling Windows.\n\n"
        L"DO NOT REPORT OR ASK FOR HELP WITH ANY ISSUES YOU ENCOUNTER.\n\n"
        L"To stop this message appearing, reinstall Windows. "
        L"This check will not be removed from OpenKneeboard."),
      _(L"OpenKneeboard"),
      MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
    return;
  } else if ((flags & OlderThanWin10) == OlderThanWin10) {
    MessageBoxW(
      nullptr,
      _(L"Your version of Windows is too old to run OpenKneeboard.\n\n"
        L"OpenKneeboard requires Windows 10 or newer. If you are using "
        L"Windows 10 or newer, turn off 'Compatibility Mode' in the shortcut "
        L"properties."),
      L"OpenKneeboard",
      MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
    return;
  } else {
    dprint.Error(
      "Damaging environment error, but no recognized flags: {:#x}",
      std::to_underlying(flags));
    return;
  }

  OPENKNEEBOARD_ASSERT((flags & ElevationRelated) == ElevationRelated);
  OPENKNEEBOARD_ASSERT(!elevationProblem.empty());

  const auto isFatal = (flags & Fatal) == Fatal;

  const auto message = std::format(
    _(L"{}; this is not supported.\n\n"
      L"Turning off User Account Control or running software as administrator "
      L"that is not intended to be ran as administrator can cause problems "
      L"that can only reasonably be fixed by reinstalling Windows.\n\n"
      L"This {} will not be removed."),
    elevationProblem,
    isFatal ? L"requirement" : L"warning");
  const auto iconFlag = isFatal ? MB_ICONERROR : MB_ICONWARNING;
  MessageBoxW(
    nullptr,
    message.c_str(),
    L"OpenKneeboard",
    MB_OK | MB_SETFOREGROUND | iconFlag);
}

[[nodiscard]]
static DamagingEnvironmentFlags LogSystemInformation() {
  dprint("{} {}", ProjectReverseDomainA, Version::ReleaseName);
  dprint(L"Full path: {}", GetFullPathForCurrentExecutable());
  dprint(L"Command line: {}", GetCommandLineW());
  dprint("----------");

  DamagingEnvironmentFlags ret = DamagingEnvironmentFlags::None;

  const auto isWine = static_cast<bool>(
    GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version"));
  const auto setWineFlags = scope_exit([&] {
    if (isWine) {
      if (
        (ret & DamagingEnvironmentFlags::Fatal)
        == DamagingEnvironmentFlags::Fatal) {
        dprint.Warning(
          "Allowing execution despite hard-blocked environment because Wine "
          "detected; assuming checks are inaccurate");
      }
      ret = DamagingEnvironmentFlags::Wine;
    }
  });

  {
    OSVERSIONINFOEXA osVersion {sizeof(OSVERSIONINFOEXA)};
    winrt::check_bool(
      GetVersionExA(reinterpret_cast<OSVERSIONINFOA*>(&osVersion)));
    if (osVersion.dwMajorVersion < 10) {
      dprint.Warning(
        "Windows {} is not supported - compatibility mode?",
        osVersion.dwMajorVersion);
      ret |= DamagingEnvironmentFlags::OlderThanWin10;
    }

    DWORD productType {};
    std::string humanMajorVer = std::to_string(osVersion.dwMajorVersion);
    if (osVersion.dwMajorVersion == 10 && osVersion.dwBuildNumber >= 22000) {
      humanMajorVer = "11";
    }

    const auto numericVersion = std::format(
      "v{}.{}.{}",
      osVersion.dwMajorVersion,
      osVersion.dwMinorVersion,
      osVersion.dwBuildNumber);

    winrt::check_bool(GetProductInfo(
      osVersion.dwMajorVersion,
      osVersion.dwMinorVersion,
      osVersion.wServicePackMajor,
      osVersion.wServicePackMinor,
      &productType));

    switch (productType) {
      case PRODUCT_CORE:
        dprint("Windows {} Home {}", humanMajorVer, numericVersion);
        break;
      case PRODUCT_PROFESSIONAL:
        dprint("Windows {} Pro {}", humanMajorVer, numericVersion);
        break;
      default:
        dprint.Warning(
          "Windows {} product {:#010x} {}",
          humanMajorVer,
          productType,
          numericVersion);
    }
  }

  dprint("----------");
  for (const auto& [label, elevated]: {
         std::tuple {"Elevated", IsElevated()},
         std::tuple {"Shell Elevated", IsShellElevated()},
       }) {
    dprint(
      "  {}: {} {}", label, elevated ? "⚠️" : "✅", elevated ? "yes" : "no");
  }

  if (IsElevated()) {
    ret |= DamagingEnvironmentFlags::IsElevated;
  }

  // Log UAC settings because lower values aren't just "do not prompt" - they
  // will automatically run some things as administrator that otherwise would
  // be ran as a normal user. This causes problems.
  if (wil::unique_hkey hkey;
      wil::reg::open_unique_key_nothrow(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        hkey)
      == ERROR_SUCCESS) {
    const auto enableLua
      = wil::reg::try_get_value_dword(hkey.get(), L"EnableLUA").value_or(0);
    const auto cpba
      = wil::reg::try_get_value_dword(hkey.get(), L"ConsentPromptBehaviorAdmin")
          .value_or(0);
    for (auto&& [name, value, isValid]: {
           std::tuple {"EnableLUA", enableLua, enableLua == 1},
           std::tuple {
             "ConsentPromptBehaviorAdmin", cpba, cpba >= 1 && cpba <= 5},
         }) {
      if (isValid) {
        dprint("  UAC {0}: ✅ {1:#010x} ({1})", name, value);
      } else {
        dprint("  UAC {0}: ⚠️ {1:#010x} ({1})", name, value);
        ret |= DamagingEnvironmentFlags::UacIsDisabled;
        if (const auto hr = wil::reg::set_value_dword_nothrow(
              HKEY_LOCAL_MACHINE,
              Config::RegistrySubKey,
              L"UacWasPreviouslyDisabled",
              1);
            FAILED(hr)) {
          dprint.Warning(
            "Failed to set UAC flag in registry: {:#010x}",
            std::bit_cast<uint32_t>(hr));
        }
      }
    }
  }
  if (wil::reg::try_get_value_dword(
        HKEY_LOCAL_MACHINE, Config::RegistrySubKey, L"UacWasPreviouslyDisabled")
        .value_or(0)) {
    dprint.Warning("UAC was previously disabled.");
    ret |= DamagingEnvironmentFlags::UacWasPreviouslyDisabled;
  }

  if (isWine) {
    ret = DamagingEnvironmentFlags::Wine;
    dprint.Warning("Wine detected; no environment checks are fatal");
  } else {
    dprint("✅ Wine not detected");
  }

  CPINFOEXW codePageInfo;
  GetCPInfoExW(CP_ACP, 0, &codePageInfo);
  dprint(L"  Active code page: {}", codePageInfo.CodePageName);
  if (codePageInfo.CodePage != CP_UTF8) {
    fatal(
      "build error (executable manifest): active code page for process is not "
      "UTF-8");
  }

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
  return ret;
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
        if (parent == utilitiesPath) {
          // Uh, yeah, this is expected at `C:\` :/
          break;
        }
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

static void OptOutOfPowerSaving() {
  dprint("Opting out of power saving for event timers (frame intervals)");
  // Windows can automatically reduce the accuracy/frequency of event timers
  // when a Window-owning process has no visible windows, including when the
  // window is fully occluded or minimized.
  //
  // In Windows 11, this changed from opt-in to opt-out. Switch it back off
  // so we can run our frame loop properly
  PROCESS_POWER_THROTTLING_STATE powerThrottling {
    .Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION,
    .ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION,
    .StateMask = 0,
  };

  if (!SetProcessInformation(
        GetCurrentProcess(),
        ProcessPowerThrottling,
        &powerThrottling,
        sizeof(powerThrottling))) {
    dprint.Warning(
      "Failed to set process power throttling state: {}",
      winrt::hresult(HRESULT_FROM_WIN32(GetLastError())));
    return;
  }
  dprint("✅ opted out of power saving event timers");
}

[[nodiscard]]
static int AppMain(
  const HINSTANCE instance,
  [[maybe_unused]] int showCommand,
  void* cefSandbox) {
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

  const auto fullDumps
    = wil::reg::try_get_value_dword(
        HKEY_LOCAL_MACHINE, Config::RegistrySubKey, L"CreateFullDumps")
        .value_or(0);
  SetDumpType(fullDumps ? DumpType::FullDump : DumpType::MiniDump);

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

  if (const auto flags = LogSystemInformation();
      flags != DamagingEnvironmentFlags::None) {
    ShowDamagingEnvironmentError(flags);
    if (
      (flags & DamagingEnvironmentFlags::Fatal)
      == DamagingEnvironmentFlags::Fatal) {
      dprint.Warning(
        L"Exiting with environment error: {:#010x}", std::to_underlying(flags));
      return EXIT_FAILURE;
    }
  }
  LogInstallationInformation();
  SetRegistryValues();

  dprint("Cleaning up temporary directories...");
  Filesystem::CleanupTemporaryDirectories();

  try {
    Filesystem::MigrateSettingsDirectory();
  } catch (const std::filesystem::filesystem_error& e) {
    dprint.Warning(
      "A filesystem error occurred while migrating settings: {:#010x}, {}",
      std::bit_cast<uint32_t>(e.code().value()),
      e.what());
  }
  try {
    BackupSettings();
  } catch (const std::filesystem::filesystem_error& e) {
    dprint.Warning(
      "A filesystem error occurred while backing up settings: {:#010x}, {}",
      std::bit_cast<uint32_t>(e.code().value()),
      e.what());
  }

  try {
    for (auto&& dir:
         {Filesystem::GetLocalAppDataDirectory(),
          Filesystem::GetSettingsDirectory()}) {
      const auto warningFile = dir / "DO_NOT_PUT_YOUR_FILES_HERE-README.txt";
      if (std::filesystem::exists(warningFile)) {
        continue;
      }

      std::ofstream f(warningFile, std::ios::trunc | std::ios::binary);
      f << "Do not put any of your files here; this directory is for "
           "OpenKneeboard's internal use, and OpenKneeboard may delete any "
           "files you put here without warning.\n\n"
           "You might want to use the My Documents folder ("
        << Filesystem::GetKnownFolderPath<FOLDERID_Documents>().string()
        << ") or a new subfolder of your user folder ("
        << Filesystem::GetKnownFolderPath<FOLDERID_Profile>().string()
        << ") instead." << std::endl;
    }
  } catch (const std::filesystem::filesystem_error& e) {
    dprint.Warning(
      "Error creating DO_NOT_PUT_YOUR_FILES_HERE files: {:#010x} {}",
      std::bit_cast<uint32_t>(e.code().value()),
      e.what());
  }

  OptOutOfPowerSaving();

  const ChromiumApp cefApp {instance, cefSandbox};

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

int __stdcall
wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
  OutputDebugStringW(
    std::format(L"OKB process: {}", GetCommandLineW()).c_str());
  {
    const auto thisExe = wil::GetModuleFileNameW();
    const auto cefPath
      = std::filesystem::path {thisExe.get()}.parent_path().parent_path()
      / "libexec" / "cef";
    SetDllDirectoryW(cefPath.c_str());
  }

  const auto cefSandbox = ChromiumApp::GetSandbox();
  if (const auto exitCode = ChromiumWorkerMain(instance, cefSandbox);
      exitCode >= 0) {
    return exitCode;
  }

  return AppMain(instance, showCommand, cefSandbox);
}
