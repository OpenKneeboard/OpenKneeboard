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
#include <OpenKneeboard/json.h>
#include <Psapi.h>
#include <Shlwapi.h>
#include <dwmapi.h>

namespace OpenKneeboard {

using WindowSpecification = WindowCaptureTab::WindowSpecification;
using MatchSpecification = WindowCaptureTab::MatchSpecification;
using TitleMatchKind = MatchSpecification::TitleMatchKind;

OPENKNEEBOARD_DECLARE_JSON(MatchSpecification);

WindowCaptureTab::WindowCaptureTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const MatchSpecification& spec)
  : WindowCaptureTab(dxr, kbs, {}, spec.mTitle, spec) {
  // Delegating constructor - you probably want nothing here
}

WindowCaptureTab::WindowCaptureTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  utf8_string_view title,
  const nlohmann::json& settings)
  : WindowCaptureTab(
    dxr,
    kbs,
    persistentID,
    title,
    settings.at("Spec").get<MatchSpecification>()) {
  // Delegating constructor - you probably want nothing here
}

WindowCaptureTab::WindowCaptureTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  utf8_string_view title,
  const MatchSpecification& spec)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mSpec(spec) {
  this->TryToStartCapture();
}

winrt::fire_and_forget WindowCaptureTab::TryToStartCapture() {
  co_await winrt::resume_background();
  const HWND desktop = GetDesktopWindow();
  HWND hwnd = NULL;
  while (hwnd = FindWindowExW(desktop, hwnd, nullptr, nullptr)) {
    const auto window = GetWindowSpecification(hwnd);
    if (!window) {
      continue;
    }

    if (mSpec.mMatchExecutable) {
      if (mSpec.mExecutable != window->mExecutable) {
        continue;
      }
    }

    if (mSpec.mMatchWindowClass) {
      if (mSpec.mWindowClass != window->mWindowClass) {
        continue;
      }
    }

    switch (mSpec.mMatchTitle) {
      case TitleMatchKind::Ignore:
        break;
      case TitleMatchKind::Exact:
        if (mSpec.mTitle != window->mTitle) {
          continue;
        }
        break;
      case TitleMatchKind::Glob: {
        const auto title = winrt::to_hstring(window->mTitle);
        const auto pattern = winrt::to_hstring(mSpec.mTitle);
        if (!PathMatchSpecW(title.c_str(), pattern.c_str())) {
          continue;
        }
        break;
      }
    }

    co_await mUIThread;
    auto source = HWNDPageSource::Create(mDXR, mKneeboard, hwnd);
    if (!(source && source->GetPageCount() > 0)) {
      continue;
    }
    this->SetDelegates({source});
    this->AddEventListener(
      source->evWindowClosedEvent,
      std::bind_front(&WindowCaptureTab::OnWindowClosed, this));
    co_return;
  }
}

WindowCaptureTab::~WindowCaptureTab() {
  this->RemoveAllEventListeners();
}

winrt::fire_and_forget WindowCaptureTab::OnWindowClosed() {
  co_await mUIThread;
  this->SetDelegates({});
  this->TryToStartCapture();
}

utf8_string WindowCaptureTab::GetGlyph() const {
  return {};
}

void WindowCaptureTab::Reload() {
}

nlohmann::json WindowCaptureTab::GetSettings() const {
  return {{"Spec", mSpec}};
}

std::unordered_map<HWND, WindowSpecification>
WindowCaptureTab::GetTopLevelWindows() {
  std::unordered_map<HWND, WindowSpecification> ret;

  const HWND desktop = GetDesktopWindow();
  HWND hwnd = NULL;
  while (hwnd = FindWindowExW(desktop, hwnd, nullptr, nullptr)) {
    auto spec = GetWindowSpecification(hwnd);
    if (spec) {
      ret[hwnd] = *spec;
    }
  }
  return ret;
}

std::optional<WindowSpecification> WindowCaptureTab::GetWindowSpecification(
  HWND hwnd) {
  // Top level windows only
  const auto style = GetWindowLongPtr(hwnd, GWL_STYLE);
  if (style & WS_CHILD) {
    return {};
  }
  // Ignore the system tray etc
  if (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
    return {};
  }

  // Ignore 'cloaked' windows:
  // https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
  if (!(style & WS_VISIBLE)) {
    // IsWindowVisible() is equivalent as we know the parent (the desktop) is
    // visible
    return {};
  }
  BOOL cloaked {false};
  if (
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))
    != S_OK) {
    return {};
  }
  if (cloaked) {
    return {};
  }

  // Ignore invisible special windows, like
  // "ApplicationManager_ImmersiveShellWindow"
  RECT rect {};
  GetWindowRect(hwnd, &rect);
  if ((rect.bottom - rect.top) == 0 || (rect.right - rect.left) == 0) {
    return {};
  }
  // Ignore 'minimized' windows, which includes both minimized and more
  // special windows...
  if (IsIconic(hwnd)) {
    return {};
  }

  wchar_t classBuf[256];
  const auto classLen = GetClassName(hwnd, classBuf, 256);
  // UWP apps like 'snip & sketch' and Windows 10's calculator
  // - can't actually capture them
  // - retrieved information isn't usable for matching
  constexpr std::wstring_view uwpClass {L"ApplicationFrameWindow"};
  if (classBuf == uwpClass) {
    return {};
  }

  DWORD processID {};
  if (!GetWindowThreadProcessId(hwnd, &processID)) {
    return {};
  }

  winrt::handle process {
    OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processID)};
  if (!process) {
    return {};
  }

  wchar_t pathBuf[MAX_PATH];
  DWORD pathLen = MAX_PATH;
  if (!QueryFullProcessImageNameW(process.get(), 0, pathBuf, &pathLen)) {
    return {};
  }

  const auto titleBufSize = GetWindowTextLengthW(hwnd) + 1;
  std::wstring titleBuf(titleBufSize, L'\0');
  const auto titleLen = GetWindowTextW(hwnd, titleBuf.data(), titleBufSize);
  titleBuf.resize(std::min(titleLen, titleBufSize));

  return WindowSpecification {
    .mExecutable = {std::wstring_view {pathBuf, pathLen}},
    .mWindowClass = winrt::to_string(
      std::wstring_view {classBuf, static_cast<size_t>(classLen)}),
    .mTitle = winrt::to_string(titleBuf),
  };
}

NLOHMANN_JSON_SERIALIZE_ENUM(
  TitleMatchKind,
  {
    {TitleMatchKind::Ignore, "Ignore"},
    {TitleMatchKind::Exact, "Exact"},
    {TitleMatchKind::Glob, "Glob"},
  })
OPENKNEEBOARD_DEFINE_JSON(
  MatchSpecification,
  mExecutable,
  mWindowClass,
  mTitle,
  mMatchTitle,
  mMatchWindowClass,
  mMatchExecutable)

}// namespace OpenKneeboard
