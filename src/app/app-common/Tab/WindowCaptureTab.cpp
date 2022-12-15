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
#include <OpenKneeboard/HWNDPageSource.h>
#include <OpenKneeboard/WindowCaptureTab.h>
#include <Psapi.h>

namespace OpenKneeboard {

namespace {

struct FindCaptureTargetData {
  std::string mTitlePattern;
  std::filesystem::path mPath;
  HWND mHwnd {};
};

BOOL EnumWindowsProc_FindCaptureTarget(HWND hwnd, LPARAM lParam) {
  auto data = reinterpret_cast<FindCaptureTargetData*>(lParam);

  DWORD processID {};
  if (!GetWindowThreadProcessId(hwnd, &processID)) {
    return TRUE;
  }

  winrt::handle process {
    OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID)};
  if (!process) {
    return TRUE;
  }

  wchar_t pathBuf[MAX_PATH];
  DWORD pathLen {MAX_PATH};
  if (!QueryFullProcessImageNameW(process.get(), 0, pathBuf, &pathLen)) {
    return true;
  }

  const std::filesystem::path path {std::wstring_view {pathBuf, pathLen}};
  // FIXME: mTitlePattern and mPath
  if (path.filename() == L"Code.exe") {
    data->mHwnd = hwnd;
    return FALSE;
  }

  return TRUE;
}

}// namespace

WindowCaptureTab::WindowCaptureTab(const DXResources& dxr, KneeboardState* kbs)
  : WindowCaptureTab(dxr, kbs, {}, L"Window Capture") {
  // Delegating constructor - you probably want nothing here
}

WindowCaptureTab::WindowCaptureTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  utf8_string_view title)
  : TabBase(persistentID, title), PageSourceWithDelegates(dxr, kbs) {
  FindCaptureTargetData targetData {};
  EnumWindows(
    &EnumWindowsProc_FindCaptureTarget, reinterpret_cast<LPARAM>(&targetData));
  const auto hwnd = targetData.mHwnd;
  if (!hwnd) {
    return;
  }

  mPageSource = std::make_shared<HWNDPageSource>(dxr, kbs, hwnd);
  this->SetDelegates({mPageSource});
}

WindowCaptureTab::~WindowCaptureTab() = default;

utf8_string WindowCaptureTab::GetGlyph() const {
  return {};
}

void WindowCaptureTab::Reload() {
}

}// namespace OpenKneeboard
