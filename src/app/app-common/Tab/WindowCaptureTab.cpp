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
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/json.h>
#include <Psapi.h>
#include <Shlwapi.h>
#include <dwmapi.h>

namespace OpenKneeboard {

using WindowSpecification = WindowCaptureTab::WindowSpecification;
using MatchSpecification = WindowCaptureTab::MatchSpecification;
using TitleMatchKind = MatchSpecification::TitleMatchKind;

std::mutex WindowCaptureTab::gHookMutex;
std::map<HWINEVENTHOOK, std::weak_ptr<WindowCaptureTab>>
  WindowCaptureTab::gHooks;

OPENKNEEBOARD_DECLARE_JSON(MatchSpecification);

struct WindowCaptureTab::Settings {
  MatchSpecification mSpec {};
  bool mSendInput = false;

  constexpr auto operator<=>(const Settings&) const noexcept = default;
};
OPENKNEEBOARD_DECLARE_JSON(WindowCaptureTab::Settings);

std::shared_ptr<WindowCaptureTab> WindowCaptureTab::Create(
  const DXResources& dxr,
  KneeboardState* kbs,
  const MatchSpecification& spec) {
  return std::shared_ptr<WindowCaptureTab>(new WindowCaptureTab(
    dxr, kbs, {}, to_utf8(spec.mExecutable.stem()), {.mSpec = spec}));
}

std::shared_ptr<WindowCaptureTab> WindowCaptureTab::Create(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& jsonSettings) {
  auto settings = jsonSettings.get<Settings>();

  auto ret = std::shared_ptr<WindowCaptureTab>(
    new WindowCaptureTab(dxr, kbs, persistentID, title, settings));
  ret->TryToStartCapture();
  return ret;
}

void WindowCaptureTab::PostCursorEvent(
  EventContext ec,
  const CursorEvent& ev,
  PageIndex pageIndex) {
  if (mSendInput) {
    PageSourceWithDelegates::PostCursorEvent(ec, ev, pageIndex);
  }
}

WindowCaptureTab::WindowCaptureTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const Settings& settings)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mSpec(settings.mSpec),
    mSendInput(settings.mSendInput) {
}

bool WindowCaptureTab::WindowMatches(HWND hwnd) const {
  const auto window = GetWindowSpecification(hwnd);
  if (!window) {
    return false;
  }

  if (mSpec.mMatchExecutable) {
    if (mSpec.mExecutable != window->mExecutable) {
      return false;
    }
  }

  if (mSpec.mMatchWindowClass) {
    if (mSpec.mWindowClass != window->mWindowClass) {
      return false;
    }
  }

  switch (mSpec.mMatchTitle) {
    case TitleMatchKind::Ignore:
      break;
    case TitleMatchKind::Exact:
      if (mSpec.mTitle != window->mTitle) {
        return false;
      }
      break;
    case TitleMatchKind::Glob: {
      const auto title = winrt::to_hstring(window->mTitle);
      const auto pattern = winrt::to_hstring(mSpec.mTitle);
      if (!PathMatchSpecW(title.c_str(), pattern.c_str())) {
        return false;
      }
      break;
    }
  }

  return true;
}

concurrency::task<bool> WindowCaptureTab::TryToStartCapture(HWND hwnd) {
  co_await mUIThread;
  auto source = HWNDPageSource::Create(mDXR, mKneeboard, hwnd);
  if (!(source && source->GetPageCount() > 0)) {
    co_return false;
  }

  dprintf(
    "Attaching to {:016x} with parent {:016x} (desktop {:016x})",
    reinterpret_cast<uint64_t>(hwnd),
    reinterpret_cast<uint64_t>(GetParent(hwnd)),
    reinterpret_cast<uint64_t>(GetDesktopWindow()));
  mDelegate = source;
  this->SetDelegates({source});
  this->AddEventListener(
    source->evWindowClosedEvent,
    std::bind_front(&WindowCaptureTab::OnWindowClosed, this));
  mHwnd = hwnd;
  co_return true;
}

winrt::fire_and_forget WindowCaptureTab::TryToStartCapture() {
  co_await winrt::resume_background();
  const HWND desktop = GetDesktopWindow();
  HWND hwnd = NULL;
  while (hwnd = FindWindowExW(desktop, hwnd, nullptr, nullptr)) {
    if (!this->WindowMatches(hwnd)) {
      continue;
    }

    if (co_await this->TryToStartCapture(hwnd)) {
      co_return;
    }
  }

  // No window :( let's set a hook
  co_await mUIThread;
  const std::unique_lock lock(gHookMutex);
  mEventHook.reset(SetWinEventHook(
    EVENT_OBJECT_CREATE,
    EVENT_OBJECT_CREATE,
    NULL,
    &WindowCaptureTab::WinEventProc_NewWindowHook,
    0,
    0,
    WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
  gHooks[mEventHook.get()] = weak_from_this();
}

WindowCaptureTab::~WindowCaptureTab() {
  this->RemoveAllEventListeners();
  std::unique_lock lock(gHookMutex);
  if (mEventHook) {
    gHooks.erase(mEventHook.get());
  }
}

winrt::fire_and_forget WindowCaptureTab::OnWindowClosed() {
  co_await mUIThread;
  this->Reload();
}

std::string WindowCaptureTab::GetGlyph() const {
  // TVMonitor
  return {"\ue7f4"};
}

void WindowCaptureTab::Reload() {
  mHwnd = {};
  mDelegate = {};
  this->SetDelegates({});
  this->TryToStartCapture();
}

nlohmann::json WindowCaptureTab::GetSettings() const {
  return Settings {
    .mSpec = mSpec,
    .mSendInput = mSendInput,
  };
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
  // Ignore the system tray etc
  if (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
    return {};
  }

  // Ignore 'cloaked' windows:
  // https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
  if (!(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_VISIBLE)) {
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

  DWORD processID {};
  if (!GetWindowThreadProcessId(hwnd, &processID)) {
    return {};
  }

  wchar_t classBuf[256];
  const auto classLen = GetClassName(hwnd, classBuf, 256);
  // UWP apps like 'snip & sketch' and Windows 10's calculator
  // - can't actually capture them
  // - retrieved information isn't usable for matching
  constexpr std::wstring_view uwpClass {L"ApplicationFrameWindow"};
  if (classBuf == uwpClass) {
    HWND child {};
    while (child = FindWindowExW(hwnd, child, nullptr, nullptr)) {
      DWORD childProcessID {};
      GetWindowThreadProcessId(child, &childProcessID);
      if (childProcessID != processID) {
        return GetWindowSpecification(child);
      }
    }
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

WindowCaptureTab::MatchSpecification WindowCaptureTab::GetMatchSpecification()
  const {
  return mSpec;
}

void WindowCaptureTab::SetMatchSpecification(const MatchSpecification& spec) {
  mSpec = spec;
  this->evSettingsChangedEvent.Emit();
  if (!this->WindowMatches(mHwnd)) {
    this->Reload();
  }
}

bool WindowCaptureTab::IsInputEnabled() const {
  return mSendInput;
}

void WindowCaptureTab::SetIsInputEnabled(bool value) {
  if (value == mSendInput) {
    return;
  }
  mSendInput = value;
  if (mSendInput) {
    mDelegate->InstallWindowHooks(mHwnd);
  }
  evSettingsChangedEvent.Emit();
}

concurrency::task<void> WindowCaptureTab::OnNewWindow(HWND hwnd) {
  if (!this->WindowMatches(hwnd)) {
    // Give new windows (especially UWP) a chance to settle before checking
    // if they match
    co_await winrt::resume_after(std::chrono::seconds(1));
    if (!this->WindowMatches(hwnd)) {
      co_return;
    }
  }

  // Sometimes we want to attach to a child (e.g. for UWP apps); make
  // sure we're actually connecting where we want in the tree
  if (!GetTopLevelWindows().contains(hwnd)) {
    co_return;
  }

  if (!co_await this->TryToStartCapture(hwnd)) {
    co_return;
  }

  const std::unique_lock lock(gHookMutex);
  gHooks.erase(mEventHook.get());
  mEventHook.reset();
}

void WindowCaptureTab::WinEventProc_NewWindowHook(
  HWINEVENTHOOK hook,
  DWORD event,
  HWND hwnd,
  LONG idObject,
  LONG idChild,
  DWORD idEventThread,
  DWORD dwmsEventTime) {
  if (event != EVENT_OBJECT_CREATE) {
    return;
  }
  if (idObject != OBJID_WINDOW) {
    return;
  }
  if (idChild != CHILDID_SELF) {
    return;
  }

  std::shared_ptr<WindowCaptureTab> instance;
  {
    std::unique_lock lock(gHookMutex);
    auto it = gHooks.find(hook);
    if (it == gHooks.end()) {
      return;
    }
    instance = it->second.lock();
    if (!instance) {
      return;
    }
  }

  [hwnd](const auto& instance) -> winrt::fire_and_forget {
    co_await instance->OnNewWindow(hwnd);
  }(instance);
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

OPENKNEEBOARD_DEFINE_JSON(WindowCaptureTab::Settings, mSpec, mSendInput);

}// namespace OpenKneeboard
