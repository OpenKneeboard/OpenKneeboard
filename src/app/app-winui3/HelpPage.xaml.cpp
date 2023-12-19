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
#include "HelpPage.xaml.h"
#include "HelpPage.g.cpp"
// clang-format on

#include "CheckForUpdates.h"
#include "FilePicker.h"

#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/SHM/ActiveConsumers.h>
#include <OpenKneeboard/Settings.h>
#include <OpenKneeboard/TroubleshootingStore.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/version.h>

#include <shims/winrt/base.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <wil/registry.h>

#include <microsoft.ui.xaml.window.h>

#include <format>
#include <fstream>
#include <string>

#include <appmodel.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <time.h>
#include <zip.h>

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

bool HelpPage::mAgreedToPrivacyWarning = false;

HelpPage::HelpPage() {
  InitializeComponent();

  this->PopulateVersion();
  this->PopulateLicenses();

  QuickStartLink().Click([](auto&, auto&) -> winrt::fire_and_forget {
    const auto quickStartPath = RuntimeFiles::GetInstallationDirectory()
      / RuntimeFiles::QUICK_START_PDF;

    co_await LaunchURI(to_utf8(quickStartPath));
  });
}

HelpPage::~HelpPage() {
  this->RemoveAllEventListeners();
}

void HelpPage::PopulateVersion() {
  UINT32 packageNameLen = MAX_PATH;
  wchar_t packageName[MAX_PATH];
  if (
    GetCurrentPackageFullName(&packageNameLen, packageName) == ERROR_SUCCESS) {
    packageNameLen -= 1;// null terminator
  } else {
    packageNameLen = 0;
  }

  auto details = std::format(
    "OpenKneeboard {}\n\n"
    "Copyright © 2021-2022 Frederick Emmott.\n\n"
    "With thanks to Paul 'Goldwolf' Whittingham for the logo and banner "
    "artwork.\n\n"
    "Build: {}-{}-{}\n"
    "Tag: {}\n"
    "Package: {}\n",
    Version::ReleaseName,
    Version::IsGithubActionsBuild ? std::format("GHA-{}", Version::Build)
                                  : "local",
    BUILD_CONFIG,
#ifdef _WIN32
#ifdef _WIN64
    "Win64",
#else
    "Win32",
#endif
#endif
    Version::IsTaggedVersion ? Version::TagName : "NONE - UNRELEASED VERSION",
    packageNameLen ? to_utf8(std::wstring_view(packageName, packageNameLen))
                   : "unpackaged");
  VersionText().Text(winrt::to_hstring(details));

  mVersionClipboardData = details;
}

static inline void SetClipboardText(std::string_view text) {
  Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(winrt::to_hstring(text));
  Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
}

static inline void SetClipboardText(std::wstring_view text) {
  Windows::ApplicationModel::DataTransfer::DataPackage package;
  package.SetText(text);
  Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
}

void HelpPage::OnCopyVersionDataClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(mVersionClipboardData);
}

void HelpPage::OnCopyGameEventsClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(GetGameEventsAsString());
}

template <class C, class T>
auto ReadableTime(const std::chrono::time_point<C, T>& time) {
  return std::chrono::zoned_time(
    std::chrono::current_zone(),
    std::chrono::time_point_cast<std::chrono::seconds>(time));
}

winrt::fire_and_forget HelpPage::OnExportClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  constexpr winrt::guid thisCall {
    0x02308bd3,
    0x2b00,
    0x4b7c,
    {0x84, 0xa8, 0x61, 0xfc, 0xcd, 0xb7, 0xe5, 0x42}};

  FilePicker picker(gMainWindow);
  picker.SettingsIdentifier(thisCall);
  picker.SuggestedStartLocation(FOLDERID_Desktop);
  picker.AppendFileType(_(L"Zip archive"), {L".zip"});
  picker.SuggestedFileName(std::format(
    L"OpenKneeboard-v{}.{}.{}.{}-{:%F-%H-%M}.zip",
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
  const scope_guard openWhenDone(
    [zipPath]() { OpenExplorerWithSelectedFile(zipPath); });

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

  AddFile("debug-log.txt", winrt::to_string(GetDPrintMessagesAsWString()));
  AddFile("api-events.txt", GetGameEventsAsString());
  AddFile("openxr.txt", GetOpenXRInfo());
  AddFile("update-history.txt", GetUpdateLog());
  AddFile("renderers.txt", GetActiveConsumers());
  AddFile("version.txt", mVersionClipboardData);

  struct Dump {
    std::filesystem::path mPath;
    std::filesystem::file_time_type mTime;
  };
  std::vector<Dump> dumps;

  const auto settingsDir = Settings::GetDirectory();
  for (const auto entry:
       std::filesystem::recursive_directory_iterator(settingsDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto path = entry.path();
    if (path.extension() == ".dmp") {
      dumps.push_back({path, entry.last_write_time()});
      continue;
    }
    if (path.extension() != ".txt" && path.extension() != ".json") {
      continue;
    }

    const auto relative = to_utf8(
      std::filesystem::proximate(path, settingsDir).generic_wstring());
    if (relative == ".instance") {
      continue;
    }

    zip_file_add(
      zip.get(),
      ("settings/" + relative).c_str(),
      zip_source_file(zip.get(), to_utf8(path).c_str(), 0, -1),
      ZIP_FL_ENC_UTF_8);
  }

  {
    wchar_t* localAppData {nullptr};
    if (
      SHGetKnownFolderPath(FOLDERID_LocalAppData, NULL, NULL, &localAppData)
        == S_OK
      && localAppData) {
      const auto crashDumps
        = std::filesystem::path(localAppData) / L"CrashDumps";
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
    AddFile("crash-dumps.txt", buffer);
  }
}

void HelpPage::OnCopyDPrintClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  SetClipboardText(GetDPrintMessagesAsWString());
}

std::string HelpPage::GetGameEventsAsString() noexcept {
  auto events = TroubleshootingStore::Get()->GetGameEvents();

  std::string ret;

  if (events.empty()) {
    ret = std::format(
      "No events as of {}", ReadableTime(std::chrono::system_clock::now()));
  }

  for (const auto& event: events) {
    ret += "\n\n";

    std::string_view name(event.mName);
    const char prefix[] = "com.fredemmott.openkneeboard";
    if (name.starts_with(prefix)) {
      name.remove_prefix(sizeof(prefix));
    }
    if (name.starts_with('.')) {
      name.remove_prefix('.');
    }

    ret += std::format(
      "{}:\n"
      "  Latest value:  '{}'\n"
      "  First seen:    {}\n"
      "  Last seen:     {}\n"
      "  Receive count: {}\n"
      "  Change count:  {}",
      name,
      event.mValue,
      ReadableTime(event.mFirstSeen),
      ReadableTime(event.mLastSeen),
      event.mReceiveCount,
      event.mUpdateCount);
  }

  return ret;
}

std::wstring HelpPage::GetDPrintMessagesAsWString() noexcept {
  auto messages = TroubleshootingStore::Get()->GetDPrintMessages();

  std::wstring ret;
  if (messages.empty()) {
    ret = L"No log messages (?!)";
  }

  bool first = true;
  for (const auto& entry: messages) {
    if (first) [[unlikely]] {
      first = false;
    } else {
      ret += L'\n';
    }

    auto exe = std::wstring_view(entry.mExecutable);
    {
      auto dirSep = exe.find_last_of(L'\\');
      if (dirSep != exe.npos && dirSep + 1 < exe.size()) {
        exe.remove_prefix(dirSep + 1);
      }
    }

    ret += std::format(
      L"[{:%F %T} {} ({})] {}: {}",
      ReadableTime(entry.mWhen),
      exe,
      entry.mProcessID,
      entry.mPrefix,
      entry.mMessage);
  }

  return ret;
}

winrt::fire_and_forget HelpPage::OnCheckForUpdatesClick(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
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
  addEntry("GNU General Public License, Version 2", docDir / "gpl-2.0.txt");

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

  auto log = [&](const auto name, const auto& value) {
    if (value == null) {
      ret += std::format("{}: inactive\n", name);
      return;
    }
    ret += std::format(
      "{}: {}\n",
      name,
      std::chrono::duration_cast<std::chrono::milliseconds>(now - value));
  };

  log("SteamVR", consumers.mSteamVR);
  log("OpenXR", consumers.mOpenXR);
  log("Oculus-D3D11", consumers.mOculusD3D11);
  log("Oculus-D3D12", consumers.mOculusD3D12);
  log("NonVR-D3D11", consumers.mNonVRD3D11);
  log("Viewer", consumers.mViewer);

  ret += std::format(
    "\nNon-VR canvas: {}x{}\n",
    consumers.mNonVRPixelSize.mWidth,
    consumers.mNonVRPixelSize.mHeight);

  return ret;
}

std::string HelpPage::GetOpenXRInfo() noexcept {
  return std::format(
    "Runtime\n=======\n\n{}\nAPI Layers\n==========\n\n64-bit "
    "HKLM\n-----------\n\n{}\n64-bit HKCU\n-----------\n\n{}",
    GetOpenXRRuntime(),
    GetOpenXRLayers(HKEY_LOCAL_MACHINE),
    GetOpenXRLayers(HKEY_CURRENT_USER));
}

std::string HelpPage::GetOpenXRLayers(HKEY root) noexcept {
  unique_hkey key;
  {
    HKEY buffer {};
    RegOpenKeyExW(
      root,
      L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit",
      0,
      KEY_READ,
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

  for (DWORD i = 0; RegEnumValueW(
                      key.get(),
                      i,
                      &path[0],
                      &pathLength,
                      nullptr,
                      &dataType,
                      reinterpret_cast<LPBYTE>(&data),
                      &dataLength)
       == ERROR_SUCCESS;
       i++,
             pathLength = static_cast<DWORD>(std::size(path)),
             dataLength = static_cast<DWORD>(sizeof(data))) {
    const auto pathUtf8
      = winrt::to_string(std::wstring_view {path, pathLength});
    if (dataType != REG_DWORD) {
      ret += std::format(
        "- '{}': INVALID REGISTRY VALUE (not DWORD)\n", pathUtf8);
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

std::string HelpPage::GetOpenXRRuntime() noexcept {
  std::string ret {"Active Runtime\n--------------\n\n"};

  try {
    const auto activeRuntime = wil::reg::get_value<std::wstring>(
      HKEY_LOCAL_MACHINE, L"SOFTWARE\\Khronos\\OpenXR\\1", L"ActiveRuntime");
    ret += to_utf8(activeRuntime) + "\n\n";
  } catch (const std::exception& e) {
    ret += std::format("FAILED TO READ FROM REGISTRY: {}\n\n", e.what());
  }

  ret += "Installed Runtimes\n------------------\n\n";

  unique_hkey key;
  {
    HKEY buffer {};
    RegOpenKeyExW(
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Khronos\\OpenXR\\1\\AvailableRuntimes",
      0,
      KEY_READ,
      &buffer);
    key.reset(buffer);
  }

  if (!key) {
    return "No available runtimes?";
  }

  wchar_t path[1024];
  auto pathLength = static_cast<DWORD>(std::size(path));
  DWORD data;
  auto dataLength = static_cast<DWORD>(sizeof(data));
  DWORD dataType;

  DWORD runtimeCount = 0;

  for (DWORD i = 0; RegEnumValueW(
                      key.get(),
                      i,
                      &path[0],
                      &pathLength,
                      nullptr,
                      &dataType,
                      reinterpret_cast<LPBYTE>(&data),
                      &dataLength)
       == ERROR_SUCCESS;
       i++,
             pathLength = static_cast<DWORD>(std::size(path)),
             dataLength = static_cast<DWORD>(sizeof(data))) {
    const auto pathUtf8
      = winrt::to_string(std::wstring_view {path, pathLength});
    if (dataType != REG_DWORD) {
      ret += std::format(
        "- '{}': INVALID REGISTRY VALUE (not DWORD)\n", pathUtf8);
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

void HelpPage::OpenExplorerWithSelectedFile(const std::filesystem::path& path) {
  PIDLIST_ABSOLUTE pidl {nullptr};
  winrt::check_hresult(
    SHParseDisplayName(path.wstring().c_str(), nullptr, &pidl, 0, nullptr));
  const scope_guard freePidl([pidl] { CoTaskMemFree(pidl); });

  winrt::check_hresult(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0));
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
