// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/WindowCaptureTab.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/json.hpp>
#include <OpenKneeboard/task/resume_after.hpp>
#include <OpenKneeboard/weak_refs.hpp>

#include <Psapi.h>
#include <Shlwapi.h>

#include <wil/resource.h>

#include <dwmapi.h>

namespace OpenKneeboard {

using WindowSpecification = WindowCaptureTab::WindowSpecification;
using MatchSpecification = WindowCaptureTab::MatchSpecification;
using TitleMatchKind = MatchSpecification::TitleMatchKind;

namespace {

auto& Instances() {
  static std::unordered_map<WindowCaptureTab*, std::weak_ptr<WindowCaptureTab>>
    value;
  return value;
}

struct WinEventHook {
 public:
  WinEventHook() {
    mHook.reset(SetWinEventHook(
      EVENT_OBJECT_CREATE,
      EVENT_OBJECT_LOCATIONCHANGE,
      nullptr,
      &WinEventHook::HookProc,
      0,
      0,
      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
  }

 private:
  static void HookProc(
    HWINEVENTHOOK,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime);

  wil::unique_hwineventhook mHook;
};

}// namespace

OPENKNEEBOARD_DECLARE_JSON(MatchSpecification);

OPENKNEEBOARD_DECLARE_JSON(HWNDPageSource::Options);
OPENKNEEBOARD_DECLARE_JSON(WindowCaptureTab::Settings);

std::shared_ptr<WindowCaptureTab> WindowCaptureTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const MatchSpecification& spec) {
  const auto tabTitle = (spec.mMatchTitle == TitleMatchKind::Exact)
    ? spec.mTitle
    : spec.mExecutableLastSeenPath.stem().string();
  Settings settings {
    .mSpec = spec,
  };
  return Create(dxr, kbs, random_guid(), tabTitle, settings);
}

std::shared_ptr<WindowCaptureTab> WindowCaptureTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& jsonSettings) {
  auto settings = jsonSettings.get<Settings>();

  auto ret = std::shared_ptr<WindowCaptureTab>(
    new WindowCaptureTab(dxr, kbs, persistentID, title, settings));
  Instances().emplace(ret.get(), ret);
  ret->TryToStartCapture();
  return ret;
}

void WindowCaptureTab::PostCursorEvent(
  KneeboardViewID ctx,
  const CursorEvent& ev,
  PageID pageID) {
  if (mSendInput) {
    PageSourceWithDelegates::PostCursorEvent(ctx, ev, pageID);
  }
}

WindowCaptureTab::WindowCaptureTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const Settings& settings)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mSpec(settings.mSpec),
    mSendInput(settings.mSendInput),
    mCaptureOptions(settings.mCaptureOptions) {
  static WinEventHook observeNewWindows;
}

bool WindowCaptureTab::WindowMatches(HWND hwnd) {
  const auto window = GetWindowSpecification(hwnd);
  if (!window) {
    return false;
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

  if (mSpec.mMatchExecutable) {
    if (mSpec.mExecutableLastSeenPath == mSpec.mExecutablePathPattern) {
      // Pattern has no wildcards
      if (window->mExecutableLastSeenPath != mSpec.mExecutableLastSeenPath) {
        return false;
      }
    }

    const auto spec = winrt::to_hstring(mSpec.mExecutablePathPattern);
    if (
      PathMatchSpecExW(
        window->mExecutableLastSeenPath.wstring().c_str(),
        spec.c_str(),
        PMSF_NORMAL | PMSF_DONT_STRIP_SPACES)
      != S_OK) {
      return false;
    }
  }

  if (mSpec.mExecutableLastSeenPath != window->mExecutableLastSeenPath) {
    mSpec.mExecutableLastSeenPath = window->mExecutableLastSeenPath;
    this->evSettingsChangedEvent.Emit();
  }

  return true;
}

task<bool> WindowCaptureTab::TryToStartCapture(HWND hwnd) {
  if (!hwnd) {
    co_return false;
  }
  if (mHwnd) {
    co_return false;
  }

  if (mPotentialHwnd.contains(hwnd)) {
    co_return false;
  }
  mPotentialHwnd.emplace(hwnd);
  const scope_exit releaser {[hwnd](auto self) {
    self->mPotentialHwnd.erase(hwnd);
  } | bind_refs_front(this) | bind_winrt_context(mUIThread)};

  auto weak = weak_from_this();

  co_await mUIThread;

  auto self = weak.lock();
  if (!self) {
    co_return false;
  }

  auto source =
    co_await HWNDPageSource::Create(mDXR, mKneeboard, hwnd, mCaptureOptions);
  if (!source) {
    co_return false;
  }

  dprint(
    "Attaching to {:016x} with parent {:016x} (desktop {:016x})",
    reinterpret_cast<uint64_t>(hwnd),
    reinterpret_cast<uint64_t>(GetParent(hwnd)),
    reinterpret_cast<uint64_t>(GetDesktopWindow()));
  if (mDelegate) {
    co_await mDelegate->DisposeAsync();
  }
  mDelegate = source;
  co_await this->SetDelegates({source});
  this->AddEventListener(
    source->evWindowClosedEvent,
    {weak_from_this(), &WindowCaptureTab::OnWindowClosed});
  mHwnd = hwnd;
  co_return true;
}

OpenKneeboard::fire_and_forget WindowCaptureTab::TryToStartCapture() {
  auto weak = weak_from_this();
  co_await winrt::resume_background();
  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  const HWND desktop = GetDesktopWindow();
  HWND hwnd = NULL;
  while ((hwnd = FindWindowExW(desktop, hwnd, nullptr, nullptr))) {
    if (!this->WindowMatches(hwnd)) {
      continue;
    }

    if (co_await this->TryToStartCapture(hwnd)) {
      co_return;
    }
  }
}

WindowCaptureTab::~WindowCaptureTab() { this->RemoveAllEventListeners(); }

OpenKneeboard::fire_and_forget WindowCaptureTab::OnWindowClosed() {
  auto weak = weak_from_this();
  co_await mUIThread;
  auto self = weak.lock();
  if (!self) {
    co_return;
  }
  co_await this->Reload();
}

std::string WindowCaptureTab::GetGlyph() const { return GetStaticGlyph(); }

task<void> WindowCaptureTab::DisposeAsync() noexcept {
  const auto disposing = co_await mDisposal.StartOnce();
  if (!disposing) {
    co_return;
  }
  Instances().erase(this);
  if (mDelegate) {
    // Should be handled by PageSourceWithDelegates, but HWNDPageSource
    // safely handles a double-dispose - so, just in case the
    // PageSourceWithDelegates is out of sync with mDelegate
    co_await mDelegate->DisposeAsync();
    mDelegate.reset();
  }
  co_await PageSourceWithDelegates::DisposeAsync();
}

std::string WindowCaptureTab::GetStaticGlyph() {
  // TVMonitor
  return {"\ue7f4"};
}

task<void> WindowCaptureTab::Reload() {
  mHwnd = {};
  if (mDelegate) {
    co_await mDelegate->DisposeAsync();
  }
  mDelegate = {};
  co_await this->SetDelegates({});
  this->TryToStartCapture();
}

nlohmann::json WindowCaptureTab::GetSettings() const {
  return Settings {
    .mSpec = mSpec,
    .mSendInput = mSendInput,
    .mCaptureOptions = mCaptureOptions,
  };
}

std::unordered_map<HWND, WindowSpecification>
WindowCaptureTab::GetTopLevelWindows() {
  std::unordered_map<HWND, WindowSpecification> ret;

  const HWND desktop = GetDesktopWindow();
  HWND hwnd = NULL;
  while ((hwnd = FindWindowExW(desktop, hwnd, nullptr, nullptr))) {
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
    while ((child = FindWindowExW(hwnd, child, nullptr, nullptr))) {
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

  const std::filesystem::path path {std::wstring_view {pathBuf, pathLen}};
  return WindowSpecification {
    .mExecutablePathPattern = path.string(),
    .mExecutableLastSeenPath = path,
    .mWindowClass = winrt::to_string(
      std::wstring_view {classBuf, static_cast<size_t>(classLen)}),
    .mTitle = winrt::to_string(titleBuf),
  };
}

WindowCaptureTab::MatchSpecification WindowCaptureTab::GetMatchSpecification()
  const {
  return mSpec;
}

task<void> WindowCaptureTab::SetMatchSpecification(
  const MatchSpecification& spec) {
  mSpec = spec;
  this->evSettingsChangedEvent.Emit();
  if (!this->WindowMatches(mHwnd)) {
    co_await this->Reload();
  }
}

bool WindowCaptureTab::IsInputEnabled() const { return mSendInput; }

void WindowCaptureTab::SetIsInputEnabled(bool value) {
  if (value == mSendInput) {
    return;
  }
  mSendInput = value;
  if (mSendInput && mDelegate) {
    mDelegate->InstallWindowHooks(mHwnd);
  }
  evSettingsChangedEvent.Emit();
}

HWNDPageSource::CaptureArea WindowCaptureTab::GetCaptureArea() const {
  return mCaptureOptions.mCaptureArea;
}

task<void> WindowCaptureTab::SetCaptureArea(CaptureArea value) {
  mCaptureOptions.mCaptureArea = value;
  evSettingsChangedEvent.Emit();
  co_await this->Reload();
}

bool WindowCaptureTab::IsCursorCaptureEnabled() const {
  return mCaptureOptions.mCaptureCursor;
}

task<void> WindowCaptureTab::SetCursorCaptureEnabled(bool value) {
  mCaptureOptions.mCaptureCursor = value;
  evSettingsChangedEvent.Emit();
  co_await this->Reload();
}

fire_and_forget WindowCaptureTab::OnNewWindow(HWND hwnd) {
  if (mHwnd) {
    co_return;
  }

  {
    const auto desktop = GetDesktopWindow();
    HWND parent {};
    while ((parent = GetParent(hwnd)) && parent != desktop) {
      hwnd = parent;
    }
  }

  // Duplicate check to early-exit and avoid enqueing up another check
  //
  // Don't add the HWND now so we attach sooner if:
  // - we don't initially match
  // - we do match when a child window is added, or when it is shown (rather
  // than created)
  if (mPotentialHwnd.contains(hwnd)) {
    co_return;
  }

  auto self = shared_from_this();

  if (!this->WindowMatches(hwnd)) {
    // Give new windows (especially UWP) a chance to settle before checking
    // if they match
    strong_ref_reseater reseater(&self);
    co_await resume_after(std::chrono::seconds(1));
    if (!reseater.reseat()) {
      co_return;
    }

    if (!this->WindowMatches(hwnd)) {
      co_return;
    }
  }

  [[maybe_unused]] auto _ = co_await this->TryToStartCapture(hwnd);
}

NLOHMANN_JSON_SERIALIZE_ENUM(
  HWNDPageSource::CaptureArea,
  {
    {HWNDPageSource::CaptureArea::FullWindow, "FullWindow"},
    {HWNDPageSource::CaptureArea::ClientArea, "ClientArea"},
  })
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  HWNDPageSource::Options,
  mCaptureArea,
  mCaptureCursor)

NLOHMANN_JSON_SERIALIZE_ENUM(
  TitleMatchKind,
  {
    {TitleMatchKind::Ignore, "Ignore"},
    {TitleMatchKind::Exact, "Exact"},
    {TitleMatchKind::Glob, "Glob"},
  })

template <>
void from_json_postprocess<MatchSpecification>(
  const nlohmann::json& j,
  MatchSpecification& m) {
  if (!m.mExecutablePathPattern.empty()) {
    return;
  }

  if (!j.contains("Executable")) {
    return;
  }

  m.mExecutablePathPattern = j.at("Executable");
  m.mExecutableLastSeenPath = m.mExecutablePathPattern;
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  MatchSpecification,
  mExecutablePathPattern,
  mExecutableLastSeenPath,
  mWindowClass,
  mTitle,
  mMatchTitle,
  mMatchWindowClass,
  mMatchExecutable)

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  WindowCaptureTab::Settings,
  mSpec,
  mSendInput,
  mCaptureOptions);

namespace {
void WinEventHook::HookProc(
  HWINEVENTHOOK,
  const DWORD event,
  HWND const hwnd,
  const LONG idObject,
  const LONG idChild,
  DWORD /* idEventThread */,
  DWORD /* dwmsEventTime*/) {
  if (
    event != EVENT_OBJECT_CREATE && event != EVENT_OBJECT_SHOW
    && event != EVENT_OBJECT_LOCATIONCHANGE) {
    return;
  }
  if (idObject != OBJID_WINDOW) {
    return;
  }
  if (idChild != CHILDID_SELF) {
    return;
  }

  // Copy in case anything enters the windows event loop recursively and
  // modifies the container while iterating
  const auto instances = Instances();
  for (auto&& weak: instances | std::views::values) {
    if (auto instance = weak.lock()) [[likely]] {
      instance->OnNewWindow(hwnd);
    } else {
      dprint.Error("Have an expired WindowCaptureTab weak_ref in hook");
    }
  }
}
}// namespace

}// namespace OpenKneeboard
