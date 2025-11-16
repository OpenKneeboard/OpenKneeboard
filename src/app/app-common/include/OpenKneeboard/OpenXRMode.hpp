// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <OpenKneeboard/task.hpp>

#include <filesystem>
#include <optional>

namespace OpenKneeboard {

enum class OpenXRMode {
  Disabled,
  AllUsers,
};

task<void> SetOpenXR64ModeWithHelperProcess(OpenXRMode newMode);

task<void> SetOpenXR32ModeWithHelperProcess(OpenXRMode newMode);
}// namespace OpenKneeboard
