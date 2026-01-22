// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <filesystem>

namespace OpenKneeboard {

enum class RunAs {
  CurrentUser,
  Administrator,
};

enum class SubprocessResult : int {
  Success,
  NonZeroExit,
  DoesNotExist,
  FailedToSpawn,
  NoProcessHandle,
};

task<SubprocessResult> RunSubprocessAsync(
  std::filesystem::path path,
  std::wstring commandLine,
  RunAs) noexcept;
};// namespace OpenKneeboard
