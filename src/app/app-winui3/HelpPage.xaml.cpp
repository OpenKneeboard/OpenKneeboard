// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "HelpPage.xaml.h"
#include "HelpPage.g.cpp"
// clang-format on

#include "CheckForUpdates.h"
#include "FilePicker.h"

#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/GameInstance.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/LaunchURI.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>
#include <OpenKneeboard/Settings.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/handles.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/utf8.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/winrt/base.h>

#include <shellapi.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <wil/registry.h>

#include <microsoft.ui.xaml.window.h>

#include <expected>
#include <format>
#include <fstream>
#include <string>

#include <shobjidl.h>
#include <time.h>
#include <zip.h>

using namespace OpenKneeboard;

namespace {
enum class RegistryView {
  Wow64_64 = 64,
  Wow64_32 = 32,
};

std::expected<std::wstring, LSTATUS> GetRegistryStringValue(
  RegistryView view,
  HKEY hkey,
  LPCWSTR subKey,
  LPCWSTR value) noexcept {
  DWORD byteCount {};
  const DWORD flags = RRF_RT_REG_SZ
    | ((view == RegistryView::Wow64_64) ? RRF_SUBKEY_WOW6464KEY
                                        : RRF_SUBKEY_WOW6432KEY);

  const auto getSizeResult
    = RegGetValueW(hkey, subKey, value, flags, nullptr, nullptr, &byteCount);
  if (getSizeResult != ERROR_SUCCESS) {
    return std::unexpected {getSizeResult};
  }

  std::wstring buffer(byteCount / sizeof(wchar_t), L'\0');
  const auto result = RegGetValueW(
    hkey, subKey, value, flags, nullptr, buffer.data(), &byteCount);
  if (result != ERROR_SUCCESS) {
    OPENKNEEBOARD_BREAK;
    return std::unexpected {result};
  }
  return {buffer};
}
}// namespace

namespace winrt::OpenKneeboardApp::implementation {

bool HelpPage::mAgreedToPrivacyWarning = false;

HelpPage::HelpPage() {
  InitializeComponent();

  this->PopulateVersion();
  this->PopulateLicenses();

  QuickStartLink().Click([]() -> OpenKneeboard::fire_and_forget {
    const auto quickStartPath = RuntimeFiles::GetInstallationDirectory()
      / RuntimeFiles::QUICK_START_PDF;

    co_await LaunchURI(to_utf8(quickStartPath));
  } | drop_winrt_event_args());
}

HelpPage::~HelpPage() {
  this->RemoveAllEventListeners();
}

void HelpPage::PopulateVersion() {
  auto details = std::format(
    "OpenKneeboard {}\n\n"
    "Copyright © 2021-2024 Frederick Emmott.\n\n"
    "With thanks to Paul 'Goldwolf' Whittingham for the logo and banner "
    "artwork.\n\n"
    "Build: {}-{}-{}\n"
    "Tag: {}\n",
    Version::ReleaseName,
    Version::IsGithubActionsBuild ? std::format("GHA{}", Version::Build)
                                  : "local",
    Config::BuildType,
#ifdef _WIN32
#ifdef _WIN64
    "Win64",
#else
    "Win32",
#endif
#endif
    Version::IsTaggedVersion ? Version::TagName : "NONE - UNRELEASED VERSION");
  VersionText().Text(winrt::to_hstring(details));

  mVersionClipboardData = details;
}

static inline void SetClipboardText(std::string_view text) {
  winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(winrt::to_hstring(text));
  winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(
    package);
}

static inline void SetClipboardText(std::wstring_view text) {
  winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(text);
  winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(
    package);
}

void HelpPage::OnCopyVersionDataClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(mVersionClipboardData);
}

template <class C, class T>
auto ReadableTime(const std::chrono::time_point<C, T>& time) {
  return std::chrono::zoned_time(
    std::chrono::current_zone(),
    std::chrono::time_point_cast<std::chrono::seconds>(time));
}

OpenKneeboard::fire_and_forget HelpPage::OnExportClick(
  IInspectable,
  RoutedEventArgs) noexcept {
  constexpr winrt::guid thisCall {
    0x02308bd3,
    0x2b00,
    0x4b7c,
    {0x84, 0xa8, 0x61, 0xfc, 0xcd, 0xb7, 0xe5, 0x42}};

  FilePicker picker(gMainWindow);
  picker.SettingsIdentifier(thisCall);
  picker.SuggestedStartLocation(FOLDERID_Desktop);
  picker.AppendFileType(_(L"Zip archive"), {L".zip"});
  picker.SuggestedFileName(
    std::format(
      L"OpenKneeboard-v{}.{}.{}.{}-{:%Y%m%dT%H%M}.zip",
      Version::Major,
      Version::Minor,
      Version::Patch,
      Version::Build,
      std::chrono::zoned_time(
        std::chrono::current_zone(), std::chrono::system_clock::now())));

  const auto maybePath = picker.PickSaveFile();
  if (!maybePath) {
    co_return;
  }
  const auto zipPath = *maybePath;
  const auto zipPathUtf8 = to_utf8(zipPath);
  const scope_exit openWhenDone(
    [zipPath]() { Filesystem::OpenExplorerWithSelectedFile(zipPath); });

  using unique_zip = std::unique_ptr<zip_t, CHandleDeleter<zip_t*, &zip_close>>;

  int ze;
  std::vector<std::string> retainedBuffers;
  unique_zip zip {
    zip_open(zipPathUtf8.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &ze)};

  auto AddFile = [zip = zip.get(), &retainedBuffers](
                   const char* name, std::string_view buffer) {
    retainedBuffers.push_back(std::string(buffer));
    const auto& copy = retainedBuffers.back();
    zip_file_add(
      zip,
      name,
      zip_source_buffer(zip, copy.data(), copy.size(), 0),
      ZIP_FL_ENC_UTF_8);
  };

  {
    const auto now = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
    const auto buffer = std::format(
      "Local time: {:%F %T%z}\n"
      "UTC time:   {:%F %T}",
      std::chrono::zoned_time(std::chrono::current_zone(), now),
      std::chrono::zoned_time("UTC", now));
    AddFile("timestamp.txt", buffer);
  }

  const auto ts = TroubleshootingStore::Get();

  AddFile("debug-log.txt", ts->GetDPrintDebugLogAsString());
  AddFile("api-events.txt", ts->GetAPIEventsDebugLogAsString());
  AddFile("openxr.txt", GetOpenXRInfo());
  AddFile("update-history.txt", GetUpdateLog());
  AddFile("renderers.txt", GetActiveConsumers());
  AddFile("version.txt", mVersionClipboardData);

  const auto settingsDir = Filesystem::GetSettingsDirectory();
  for (const auto entry:
       std::filesystem::recursive_directory_iterator(settingsDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto path = entry.path();
    if (path.extension() == ".dmp") {
      continue;
    }
    if (path.extension() != ".txt" && path.extension() != ".json") {
      continue;
    }

    const auto relative = to_utf8(
      std::filesystem::proximate(path, settingsDir).generic_wstring());
    // v1.8 and below; PID etc. Now in %LOCALAPPDATA% / OpenKneeboard /
    // instance.txt
    if (relative == ".instance") {
      continue;
    }

    zip_file_add(
      zip.get(),
      ("settings/" + relative).c_str(),
      zip_source_file(zip.get(), to_utf8(path).c_str(), 0, ZIP_LENGTH_TO_END),
      ZIP_FL_ENC_UTF_8);
  }

  struct Dump {
    std::filesystem::path mPath;
    std::filesystem::file_time_type mTime;
  };
  std::vector<Dump> dumps;

  {
    const auto crashDumps
      = Filesystem::GetKnownFolderPath<FOLDERID_LocalAppData>() / L"CrashDumps";
    if (std::filesystem::is_directory(crashDumps)) {
      for (const auto entry:
           std::filesystem::recursive_directory_iterator(crashDumps)) {
        if (entry.path().filename().wstring().starts_with(
              L"OpenKneeboardApp.exe")) {
          dumps.push_back({entry.path(), entry.last_write_time()});
        }
      }
    }
  }
  for (const auto entry: std::filesystem::directory_iterator(
         Filesystem::GetCrashLogsDirectory())) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const auto path = entry.path();
    if (path.extension() == ".dmp") {
      dumps.push_back({entry.path(), entry.last_write_time()});
      continue;
    }

    if (path.extension() != ".txt") {
      OPENKNEEBOARD_BREAK;
      continue;
    }

    zip_file_add(
      zip.get(),
      ("crashes" / path.filename()).string().c_str(),
      zip_source_file(zip.get(), to_utf8(path).c_str(), 0, -1),
      ZIP_FL_ENC_UTF_8);
  }

  if (!dumps.empty()) {
    std::sort(dumps.begin(), dumps.end(), [](const auto& a, const auto& b) {
      return a.mTime < b.mTime;
    });
    std::string buffer;
    for (const auto& dump: dumps) {
      const auto time = ReadableTime(
        std::chrono::clock_cast<std::chrono::system_clock>(dump.mTime));
      buffer += std::format(
        "- [{:%F %T}] {} ({} bytes)\n",
        time,
        to_utf8(dump.mPath),
        std::filesystem::file_size(dump.mPath));
    }
    AddFile("crashes/dumps.txt", buffer);
  }
}

OpenKneeboard::fire_and_forget HelpPage::OnCheckForUpdatesClick(
  IInspectable,
  RoutedEventArgs) noexcept {
  co_await CheckForUpdates(UpdateCheckType::Manual, this->XamlRoot());
}

void HelpPage::PopulateLicenses() noexcept {
  const auto docDir
    = Filesystem::GetRuntimeDirectory().parent_path() / "share" / "doc";

  if (!std::filesystem::exists(docDir)) {
    return;
  }

  auto children = Licenses().Children();
  children.Clear();

  auto addEntry
    = [&](const std::string& label, const std::filesystem::path& path) {
        Controls::HyperlinkButton link;
        link.Content(box_value(to_hstring(label)));
        link.Click(
          [=](const auto&, const auto&) { this->DisplayLicense(label, path); });
        children.Append(link);
      };

  addEntry("OpenKneeboard", docDir / "LICENSE.txt");

  Controls::TextBlock ackBlock;
  ackBlock.TextWrapping(TextWrapping::WrapWholeWords);
  ackBlock.Text(
    _(L"OpenKneeboard uses and includes software from the following "
      L"projects:"));
  children.Append(ackBlock);

  for (const auto& entry: std::filesystem::directory_iterator(docDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto label = to_utf8(entry.path().stem());
    if (!label.starts_with("LICENSE-ThirdParty-")) {
      continue;
    }
    label.erase(0, sizeof("LICENSE-ThirdParty-") - 1 /* trailing null */);

    addEntry(label, entry.path());
  }
}

std::string HelpPage::GetUpdateLog() noexcept {
  unique_hkey key;
  {
    HKEY buffer {};
    RegOpenKeyExW(
      HKEY_CURRENT_USER,
      std::format(L"{}\\Updates", RegistrySubKey).c_str(),
      0,
      KEY_READ,
      &buffer);
    key.reset(buffer);
  }

  if (!key) {
    return {"No update log in registry."};
  }

  std::string ret {"Update log:\n\n"};

  char valueName[1024];
  auto valueNameLength = static_cast<DWORD>(std::size(valueName));
  char data[1024];
  auto dataLength = static_cast<DWORD>(std::size(data));

  for (DWORD i = 0; RegEnumValueA(
                      key.get(),
                      i,
                      &valueName[0],
                      &valueNameLength,
                      nullptr,
                      nullptr,
                      reinterpret_cast<LPBYTE>(&data[0]),
                      &dataLength)
       == ERROR_SUCCESS;
       i++,
             valueNameLength = static_cast<DWORD>(std::size(valueName)),
             dataLength = static_cast<DWORD>(std::size(data))) {
    const auto timestamp
      = std::stoull(std::string {valueName, valueNameLength});
    ret += std::format(
      "- {:%F %T%z}: {}\n",
      ReadableTime(
        std::chrono::system_clock::time_point(std::chrono::seconds(timestamp))),
      std::string_view {data, dataLength - 1});
  }
  return ret;
}

std::string HelpPage::GetActiveConsumers() noexcept {
  std::string ret;
  const auto consumers = SHM::ActiveConsumers::Get();
  const auto now = SHM::ActiveConsumers::Clock::now();
  const SHM::ActiveConsumers::T null {};

  const auto log = [&](const auto name, const auto& value) {
    if (value == null) {
      ret += std::format("{}: inactive\n", name);
      return;
    }
    ret += std::format(
      "{}: {}\n",
      name,
      std::chrono::duration_cast<std::chrono::milliseconds>(now - value));
  };

  log("OpenVR", consumers.mOpenVR);
  log("OpenXR", consumers.mOpenXR);
  log("Oculus-D3D11", consumers.mOculusD3D11);
  log("NonVR-D3D11", consumers.mNonVRD3D11);
  log("Viewer", consumers.mViewer);

  ret += std::format(
    "\nNon-VR canvas: {}x{}\n\n",
    consumers.mNonVRPixelSize.mWidth,
    consumers.mNonVRPixelSize.mHeight);

  const auto logGame
    = [&](const auto name, const std::optional<GameProcess>& process) {
        if (!process) {
          ret += std::format("{}: none\n", name);
          return;
        }
        const auto info = process->mGameInstance.lock();
        if (info) {
          ret += std::format(
            "{}: {} (PID {}, started {} ago)\n",
            name,
            info->mLastSeenPath.string(),
            process->mProcessID,
            std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now() - process->mSince));
        } else {
          ret += std::format("{}: {}\n", name, process->mProcessID);
        }
      };

  const auto kb = gKneeboard.lock();
  logGame("Current game", kb->GetCurrentGame());
  logGame("Most recent game", kb->GetMostRecentGame());

  return ret;
}

static std::string GetOpenXRRuntime(RegistryView view) noexcept {
  std::string ret = std::format(
    "Active {}-bit Runtime\n--------------\n\n", static_cast<int>(view));

  const auto activeRuntime = GetRegistryStringValue(
    view,
    HKEY_LOCAL_MACHINE,
    L"SOFTWARE\\Khronos\\OpenXR\\1",
    L"ActiveRuntime");
  if (activeRuntime.has_value()) {
    ret += to_utf8(activeRuntime.value()) + "\n\n";
  } else {
    ret += std::format(
      "FAILED TO READ FROM REGISTRY: {:#x}\n\n",
      static_cast<uint32_t>(activeRuntime.error()));
  }

  ret += std::format(
    "Installed {}-bit Runtimes\n------------------\n\n",
    static_cast<int>(view));

  unique_hkey key;
  {
    HKEY buffer {};
    RegOpenKeyExW(
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Khronos\\OpenXR\\1\\AvailableRuntimes",
      0,
      KEY_READ
        | ((view == RegistryView::Wow64_64) ? KEY_WOW64_64KEY : KEY_WOW64_32KEY),
      &buffer);
    key.reset(buffer);
  }

  if (!key) {
    // Registering an 'available runtime' is a *should*, and some runtimes
    // don't.
    //
    // It's possible to set an active runtime without setting an available
    // runtime, so this can fail, but we still have useful information.
    ret += "No available runtimes?";
    return ret;
  }

  wchar_t path[1024];
  auto pathLength = static_cast<DWORD>(std::size(path));
  DWORD data;
  auto dataLength = static_cast<DWORD>(sizeof(data));
  DWORD dataType;

  DWORD runtimeCount = 0;
  bool moreRuntimes = true;

  for (DWORD i = 0; moreRuntimes; i++,
             pathLength = static_cast<DWORD>(std::size(path)),
             dataLength = static_cast<DWORD>(sizeof(data))) {
    switch (RegEnumValueW(
      key.get(),
      i,
      &path[0],
      &pathLength,
      nullptr,
      &dataType,
      reinterpret_cast<LPBYTE>(&data),
      &dataLength)) {
      case ERROR_SUCCESS:
        break;
      case ERROR_NO_MORE_ITEMS:
        moreRuntimes = false;
        continue;
        break;
      case ERROR_MORE_DATA:
        // Bigger than DWORD means not a DWORD; handled by REG_DWORD check below
        break;
    }

    const auto pathUtf8
      = winrt::to_string(std::wstring_view {path, pathLength});

    if (dataType != REG_DWORD) {
      ret += std::format(
        "- ERROR - INVALID REGISTRY VALUE (not DWORD): {}\n", pathUtf8);
      continue;
    }
    const auto disabled = static_cast<bool>(data);
    if (disabled) {
      ret += std::format("- DISABLED: {}\n", pathUtf8);
      continue;
    }

    const auto fspath
      = std::filesystem::path(std::wstring_view {path, pathLength});
    if (!std::filesystem::exists(fspath)) {
      ret += std::format("- FILE DOES NOT EXIST: {}\n", pathUtf8);
      continue;
    }

    nlohmann::json json;
    {
      std::ifstream f(fspath.c_str());
      f >> json;
    }
    const auto runtime = json.at("runtime");

    const auto dllPathStr = runtime.at("library_path").get<std::string>();
    std::filesystem::path dllPath(dllPathStr);
    if (dllPath.is_relative()) {
      dllPath = (fspath.parent_path() / dllPath).lexically_normal();
    }

    const auto runtimeName
      = (runtime.contains("name") && runtime.at("name").is_string())
      ? runtime.at("name").get<std::string>()
      : "Unnamed Runtime";
    ret += std::format(
      "- #{}: {}\n    - DLL: {}\n    - JSON: {}\n",
      ++runtimeCount,
      runtimeName,
      to_utf8(dllPath),
      pathUtf8);
  }

  return ret;
}

std::string GetOpenXRLayers(RegistryView view, HKEY root) noexcept {
  unique_hkey key;
  {
    HKEY buffer {};
    RegOpenKeyExW(
      root,
      L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit",
      0,
      KEY_READ
        | ((view == RegistryView::Wow64_64) ? KEY_WOW64_64KEY : KEY_WOW64_32KEY),
      &buffer);
    key.reset(buffer);
  }

  if (!key) {
    return "No layers.\n";
  }

  std::string ret;

  wchar_t path[1024];
  auto pathLength = static_cast<DWORD>(std::size(path));
  DWORD data;
  auto dataLength = static_cast<DWORD>(sizeof(data));
  DWORD dataType;

  size_t extensionCount = 0;

  bool moreLayers = true;
  for (DWORD i = 0; moreLayers; i++,
             pathLength = static_cast<DWORD>(std::size(path)),
             dataLength = static_cast<DWORD>(sizeof(data))) {
    switch (RegEnumValueW(
      key.get(),
      i,
      &path[0],
      &pathLength,
      nullptr,
      &dataType,
      reinterpret_cast<LPBYTE>(&data),
      &dataLength)) {
      case ERROR_SUCCESS:
        break;
      case ERROR_MORE_DATA:
        break;
      case ERROR_NO_MORE_ITEMS:
        moreLayers = false;
        continue;
    }
    const auto pathUtf8
      = winrt::to_string(std::wstring_view {path, pathLength});
    if (dataType != REG_DWORD) {
      ret += std::format(
        "- ERROR - INVALID REGISTRY VALUE (not DWORD): {}\n", pathUtf8);
      continue;
    }
    const auto disabled = static_cast<bool>(data);
    if (disabled) {
      ret += std::format("- DISABLED: {}\n", pathUtf8);
      continue;
    }

    const auto fspath
      = std::filesystem::path(std::wstring_view {path, pathLength});
    if (!std::filesystem::exists(fspath)) {
      ret += std::format("- FILE DOES NOT EXIST: {}\n", pathUtf8);
      continue;
    }

    nlohmann::json json;
    {
      std::ifstream f(fspath.c_str());
      f >> json;
    }
    const auto layer = json.at("api_layer");

    const auto dllPathStr = layer.at("library_path").get<std::string>();
    std::filesystem::path dllPath(dllPathStr);
    if (dllPath.is_relative()) {
      dllPath = (fspath.parent_path() / dllPath).lexically_normal();
    }

    ret += std::format(
      "- #{}: {}\n    {}\n    - DLL: {}\n    - JSON: {}\n    "
      "- Version: {}\n",
      ++extensionCount,
      layer.at("name").get<std::string>(),
      layer.at("description").get<std::string>(),
      to_utf8(dllPath),
      pathUtf8,
      layer.at("implementation_version").get<std::string>());
  }
  return ret;
}

std::string HelpPage::GetOpenXRInfo() noexcept {
  return std::format(
    "64-bit Runtime\n=======\n\n{}"
    "\n\n32-bit Runtime\n=======\n\n{}"
    "\n\nAPI Layers\n=========="
    "\n\n64-bit "
    "HKLM\n-----------\n\n{}\n64-bit HKCU\n-----------\n\n{}"
    "\n\n32-bit "
    "HKLM\n-----------\n\n{}\n32-bit HKCU\n-----------\n\n{}",
    GetOpenXRRuntime(RegistryView::Wow64_64),
    GetOpenXRRuntime(RegistryView::Wow64_32),
    GetOpenXRLayers(RegistryView::Wow64_64, HKEY_LOCAL_MACHINE),
    GetOpenXRLayers(RegistryView::Wow64_64, HKEY_CURRENT_USER),
    GetOpenXRLayers(RegistryView::Wow64_32, HKEY_LOCAL_MACHINE),
    GetOpenXRLayers(RegistryView::Wow64_32, HKEY_CURRENT_USER));
}

bool HelpPage::AgreeButtonIsEnabled() noexcept {
  return !mAgreedToPrivacyWarning;
}

bool HelpPage::AgreedToPrivacyWarning() noexcept {
  return mAgreedToPrivacyWarning;
}

void HelpPage::OnAgreeClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  if (mAgreedToPrivacyWarning) {
    return;
  }
  mAgreedToPrivacyWarning = true;
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

void HelpPage::DisplayLicense(
  const std::string& /* title */,
  const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    return;
  }

  ShellExecuteW(NULL, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}// namespace winrt::OpenKneeboardApp::implementation
