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
#include <OpenKneeboard/GetMainHWND.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <shims/winrt/base.h>

#include <format>

namespace OpenKneeboard {

static struct {
  std::optional<MainWindowInfo> mInfo {};
  std::chrono::time_point<std::chrono::steady_clock> mCacheTime {};
} gCache;

std::optional<MainWindowInfo> GetMainWindowInfo() {
  const auto now = std::chrono::steady_clock::now();
  if (now - gCache.mCacheTime < std::chrono::seconds(1)) {
    return gCache.mInfo;
  }
  const auto cached = gCache.mInfo;
  gCache.mInfo = {};
  const scope_guard updateCacheTime([&]() { gCache.mCacheTime = now; });

  auto name = std::format(L"Local\\{}.hwnd", OpenKneeboard::ProjectNameW);
  winrt::handle hwndFile {OpenFileMapping(PAGE_READWRITE, FALSE, name.c_str())};
  if (!hwndFile) {
    return {};
  }

  void* mapping = MapViewOfFile(
    hwndFile.get(), FILE_MAP_READ, 0, 0, sizeof(MainWindowInfo));
  if (!mapping) {
    return {};
  }
  scope_guard closeView([=]() { UnmapViewOfFile(mapping); });

  MEMORY_BASIC_INFORMATION mappingInfo;
  VirtualQuery(GetCurrentProcess(), &mappingInfo, sizeof(mappingInfo));
  if (mappingInfo.RegionSize == sizeof(HWND)) {
    gCache.mInfo = MainWindowInfo {.mHwnd = *reinterpret_cast<HWND*>(mapping)};
    if (cached != gCache.mInfo) {
      dprint("Found an existing window with no version information");
    }
    return gCache.mInfo;
  }

  gCache.mInfo = *reinterpret_cast<MainWindowInfo*>(mapping);
  if (cached != gCache.mInfo) {
    const auto version = gCache.mInfo->mVersion;
    dprintf(
      "Found an existing window for v{}.{}.{}.{}",
      version.mMajor,
      version.mMinor,
      version.mPatch,
      version.mBuild);
  }
  return gCache.mInfo;
}

std::optional<HWND> GetMainHWND() {
  auto info = GetMainWindowInfo();
  if (info) {
    return info->mHwnd;
  }
  return {};
}

}// namespace OpenKneeboard
