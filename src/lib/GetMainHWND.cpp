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

#include <OpenKneeboard/GetMainHWND.h>
#include <OpenKneeboard/config.h>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <shims/winrt.h>

namespace OpenKneeboard {

std::optional<HWND> GetMainHWND() {
  auto name = fmt::format(L"Local\\{}.hwnd", OpenKneeboard::ProjectNameW);
  winrt::handle hwndFile {OpenFileMapping(PAGE_READWRITE, FALSE, name.c_str())};
  if (!hwndFile) {
    return {};
  }
  void* mapping
    = MapViewOfFile(hwndFile.get(), FILE_MAP_READ, 0, 0, sizeof(HWND));
  if (!mapping) {
    return {};
  }
  auto hwnd = *reinterpret_cast<HWND*>(mapping);
  UnmapViewOfFile(mapping);
  return {hwnd};
}

}// namespace OpenKneeboard
