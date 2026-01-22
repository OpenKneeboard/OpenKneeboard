// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "App.xaml.g.h"

#include <OpenKneeboard/task.hpp>

namespace winrt::OpenKneeboardApp::implementation {
struct App : AppT<App> {
  App();

  OpenKneeboard::fire_and_forget OnLaunched(
    Microsoft::UI::Xaml::LaunchActivatedEventArgs) noexcept;

  ::OpenKneeboard::task<void> CleanupAndExitAsync();

 private:
  winrt::Microsoft::UI::Xaml::Window mWindow {nullptr};
};
}// namespace winrt::OpenKneeboardApp::implementation
