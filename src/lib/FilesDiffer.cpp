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
#include <OpenKneeboard/FilesDiffer.h>
#include <OpenKneeboard/Win32.h>

#include <shims/filesystem>
#include <shims/winrt/base.h>

#include <algorithm>

namespace OpenKneeboard {

bool FilesDiffer(
  const std::filesystem::path& a,
  const std::filesystem::path& b) {
  const auto aExists = std::filesystem::exists(a);
  const auto bExists = std::filesystem::exists(b);
  if (aExists != bExists) {
    return true;
  }
  if (!aExists) {
    // neither exists
    return false;
  }

  const auto size = std::filesystem::file_size(a);
  if (std::filesystem::file_size(b) != size) {
    return true;
  }

  const auto af = Win32::CreateFileW(
    a.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  const auto bf = Win32::CreateFileW(
    b.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);

  const auto afm = Win32::CreateFileMappingW(
    af.get(), nullptr, PAGE_READONLY, 0, 0, nullptr);
  const auto bfm = Win32::CreateFileMappingW(
    bf.get(), nullptr, PAGE_READONLY, 0, 0, nullptr);

  auto av = reinterpret_cast<std::byte*>(
    MapViewOfFile(afm.get(), FILE_MAP_READ, 0, 0, 0));
  auto bv = reinterpret_cast<std::byte*>(
    MapViewOfFile(bfm.get(), FILE_MAP_READ, 0, 0, 0));

  const auto equal = std::equal(av, av + size, bv, bv + size);

  UnmapViewOfFile(av);
  UnmapViewOfFile(bv);

  return !equal;
}

}// namespace OpenKneeboard
