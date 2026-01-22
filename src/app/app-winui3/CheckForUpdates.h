// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <winrt/microsoft.ui.xaml.h>

#include <future>

namespace OpenKneeboard {

enum class UpdateCheckType {
  Automatic,
  Manual,
};

enum class UpdateResult {
  InstallingUpdate,
  NotInstallingUpdate,
};

task<UpdateResult> CheckForUpdates(
  UpdateCheckType,
  winrt::Microsoft::UI::Xaml::XamlRoot);

}// namespace OpenKneeboard
