// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/FilesDiffer.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <shims/winrt/base.h>

#include <algorithm>
#include <filesystem>

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

  const auto af = Win32::or_throw::CreateFile(
    a.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  const auto bf = Win32::or_throw::CreateFile(
    b.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);

  const auto afm = Win32::or_throw::CreateFileMapping(
    af.get(), nullptr, PAGE_READONLY, 0, 0, nullptr);
  const auto bfm = Win32::or_throw::CreateFileMapping(
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
