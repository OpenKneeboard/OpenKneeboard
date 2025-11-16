// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <Windows.h>

namespace OpenKneeboard {

bool IsElevated(HANDLE process) noexcept;
bool IsElevated() noexcept;
/// If the shell is elevated, we can't de-elevate, and UAC is probably disabled.
bool IsShellElevated() noexcept;

}// namespace OpenKneeboard
