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

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <Psapi.h>

#include <filesystem>

#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Navigation;
using namespace OpenKneeboardApp::implementation;
using namespace OpenKneeboardApp;

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
  OpenKneeboard::DPrintSettings::Set({
    .prefix = "OpenKneeboard-WinUI3",
  });

  if (ActivateExistingInstance()) {
    return 0;
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

  return 0;
}
