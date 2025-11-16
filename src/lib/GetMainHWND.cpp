// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/GetMainHWND.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

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
  const scope_exit updateCacheTime([&]() { gCache.mCacheTime = now; });

  auto name
    = std::format(L"Local\\{}.hwnd", OpenKneeboard::ProjectReverseDomainW);
  winrt::handle hwndFile {OpenFileMapping(PAGE_READWRITE, FALSE, name.c_str())};
  if (!hwndFile) {
    return {};
  }

  void* mapping = MapViewOfFile(
    hwndFile.get(), FILE_MAP_READ, 0, 0, sizeof(MainWindowInfo));
  if (!mapping) {
    return {};
  }
  scope_exit closeView([=]() { UnmapViewOfFile(mapping); });

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
    dprint(
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
