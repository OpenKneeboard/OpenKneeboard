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
#pragma once

#include <Windows.h>

namespace OpenKneeboard {

bool IsElevated(HANDLE process) noexcept;
bool IsElevated() noexcept;
/// If the shell is elevated, we can't de-elevate, and UAC is probably disabled.
bool IsShellElevated() noexcept;

enum class DesiredElevation : int {
  KeepInitialPrivileges = 0,
  Elevated = 1,
  NotElevated = 2,
};

DesiredElevation GetDesiredElevation() noexcept;

// Throws unless elevated; should be called from an elevated helper.
void SetDesiredElevation(DesiredElevation);

bool RelaunchWithDesiredElevation(DesiredElevation, int showCommand);

}// namespace OpenKneeboard
