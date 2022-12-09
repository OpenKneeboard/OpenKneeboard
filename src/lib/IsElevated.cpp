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
#include <OpenKneeboard/IsElevated.h>
#include <shims/winrt/base.h>

namespace OpenKneeboard {

bool IsElevated() noexcept {
  winrt::handle token;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put())) {
    return false;
  }
  TOKEN_ELEVATION elevation {};
  DWORD elevationSize = sizeof(TOKEN_ELEVATION);
  if (!GetTokenInformation(
        token.get(),
        TokenElevation,
        &elevation,
        sizeof(elevation),
        &elevationSize)) {
    return false;
  }
  return elevation.TokenIsElevated;
}

}// namespace OpenKneeboard
