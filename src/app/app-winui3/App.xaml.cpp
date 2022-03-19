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

#include <OpenKneeboard/DCSWorldInstance.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <Psapi.h>
#include <appmodel.h>

#include <set>
#include <filesystem>

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

App::App() {
  InitializeComponent();

#if defined _DEBUG \
  && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
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

void App::OnLaunched(LaunchActivatedEventArgs const&) {
  window = make<MainWindow>();

  window.Content().as<winrt::Microsoft::UI::Xaml::FrameworkElement>().Loaded(
    [=](auto&, auto&) -> winrt::fire_and_forget {

  std::set<std::filesystem::path> dcsSavedGamesPaths;
  for (const auto& game: gKneeboard->GetGamesList()->GetGameInstances()) {
    auto dcs = std::dynamic_pointer_cast<DCSWorldInstance>(game);
    if (!dcs) {
      continue;
    }
    const auto path = dcs->mSavedGamesPath;
    if (!dcsSavedGamesPaths.contains(path)) {
      dcsSavedGamesPaths.emplace(path);
    }
  }

  auto root = window.Content().XamlRoot();
  for (const auto& path: dcsSavedGamesPaths) {
    co_await CheckDCSHooks(root, path);
  }
    });

  window.Activate();
}

static bool ActivateExistingInstance() {
  // If already running, make the other instance active.
  //
  // There's more common ways to do this, but given we already have the SHM
  // and the SHM has a handy HWND, might as well use that.
  OpenKneeboard::SHM::Reader shm;
  if (!shm) {
    return false;
  }
  auto snapshot = shm.MaybeGet();
  if (!snapshot) {
    return false;
  }
  const auto hwnd = snapshot.GetConfig().feederWindow;
  if (!hwnd) {
    return false;
  }
  DWORD processID = 0;
  GetWindowThreadProcessId(hwnd, &processID);
  if (!processID) {
    return false;
  }
  winrt::handle processHandle {
    OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processID)};
  if (!processHandle) {
    return false;
  }
  wchar_t processName[MAX_PATH];
  auto processNameLen
    = GetProcessImageFileNameW(processHandle.get(), processName, MAX_PATH);
  const auto processStem
    = std::filesystem::path(std::wstring_view(processName, processNameLen))
        .stem();
  processNameLen = GetModuleFileNameW(NULL, processName, MAX_PATH);
  const auto thisProcessStem
    = std::filesystem::path(std::wstring_view(processName, processNameLen))
        .stem();
  if (processStem != thisProcessStem) {
    return false;
  }
  return ShowWindow(hwnd, SW_SHOWNORMAL) && SetForegroundWindow(hwnd);
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  DPrintSettings::Set({
    .prefix = "OpenKneeboard-WinUI3",
  });

  if (ActivateExistingInstance()) {
    return 0;
  }

  UINT32 packageNameLen = MAX_PATH;
  wchar_t packageName[MAX_PATH];
  const bool isPackaged
    = GetCurrentPackageFullName(&packageNameLen, packageName) == ERROR_SUCCESS;
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
