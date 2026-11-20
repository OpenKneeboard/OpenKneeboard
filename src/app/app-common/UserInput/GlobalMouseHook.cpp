// OpenKneeboard
//
// Copyright (c) 2026 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/GlobalMouseHook.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

namespace OpenKneeboard {

namespace {
GlobalMouseHook*& Instance() {
  thread_local GlobalMouseHook* instance = nullptr;
  return instance;
}

[[nodiscard]]
bool& EscapeHatch() {
  static bool disabled {};
  return disabled;
}

[[nodiscard]]
LARGE_INTEGER EscapeHatchTimeoutTicks() {
  static LARGE_INTEGER ticksPerSecond = []() {
    LARGE_INTEGER ret {};
    QueryPerformanceFrequency(&ret);
    // ret now contains number of ticks in 1 second; increase to 3 seconds
    ret.QuadPart *= 3;
    return ret;
  }();
  return ticksPerSecond;
}
}// namespace

LRESULT CALLBACK GlobalMouseHook::MouseCallback(
  const int nCode,
  const WPARAM wParam,
  const LPARAM lParam) noexcept {
  OPENKNEEBOARD_ASSERT(Instance());
  if (Instance() && nCode >= 0) {
    Instance()->OnMouseMessage(
      wParam, reinterpret_cast<MSLLHOOKSTRUCT*>(lParam));
    return 1;// Do not propagate
  }

  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK GlobalMouseHook::KeyboardCallback(
  const int nCode,
  const WPARAM wParam,
  const LPARAM lParam) noexcept {
  OPENKNEEBOARD_ASSERT(Instance());
  if (Instance() && nCode >= 0) {
    Instance()->OnKeyboardMessage(
      wParam, reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam));
  }

  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

GlobalMouseHook::GlobalMouseHook(KneeboardState* kneeboard)
  : mKneeboard(kneeboard) {
  OPENKNEEBOARD_ASSERT(mKneeboard);

  if (EscapeHatch()) {
    return;
  }

  OPENKNEEBOARD_ASSERT(!Instance());

  GetCursorPos(&mOrigin);
  using Mask = CursorEvent::MouseButtonsMask;
  if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
    mButtons |= std::to_underlying(Mask::Left);
  }
  if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {
    mButtons |= std::to_underlying(Mask::Middle);
  }
  if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
    mButtons |= std::to_underlying(Mask::Right);
  }
  if (mButtons) {
    // Don't start capture if a button is already pressed, e.g. when dragging
    // something
    return;
  }

  mMouseHook = SetWindowsHookExW(WH_MOUSE_LL, &MouseCallback, nullptr, 0);

  if (!mMouseHook) {
    dprint.Warning("Failed to install global mouse hook: {}", GetLastError());
    return;
  }

  mKeyboardHook =
    SetWindowsHookExW(WH_KEYBOARD_LL, &KeyboardCallback, nullptr, 0);
  if (!mKeyboardHook) {
    dprint.Warning(
      "Failed to install global keyboard hook: {}", GetLastError());
    if (!UnhookWindowsHookEx(mMouseHook)) {
      fatal("Failed to remove global mouse hook: {}", GetLastError());
    }
    mMouseHook = nullptr;
    return;
  }

  Instance() = this;
}

GlobalMouseHook::~GlobalMouseHook() { Unhook(); }

GlobalMouseHook GlobalMouseHook::Get(KneeboardState* kneeboard) {
  OPENKNEEBOARD_ALWAYS_ASSERT(
    !Instance(), "Can't have multiple simultanous global mouse hooks");
  return GlobalMouseHook {kneeboard};
}

void GlobalMouseHook::Unhook() noexcept {
  if (!mMouseHook) {
    return;
  }

  Instance() = nullptr;

  if (!UnhookWindowsHookEx(mMouseHook)) [[unlikely]] {
    // Exit process immediately so windows uninstalls the hook
    fatal("Failed to remove global mouse hook: {}", GetLastError());
  }

  if (!UnhookWindowsHookEx(mKeyboardHook)) [[unlikely]] {
    fatal("Failed to remove global keyboard hook: {}", GetLastError());
  }

  mMouseHook = nullptr;
  mKeyboardHook = nullptr;
}

void GlobalMouseHook::OnMouseMessage(
  const WPARAM message,
  MSLLHOOKSTRUCT* const data) {
  const auto target = mKneeboard->GetViewForGlobalInput();
  if (!target) {
    return;
  }

  const auto prevPoint = target->GetMostRecentCursorCanvasPoint().value_or(
    Geometry2D::Point {0.5f, 0.5f});
  const auto targetSize = target->GetPreferredSize();
  const auto scale =
    1.0f
    / std::max(
      {1ui32, targetSize.mPixelSize.mWidth, targetSize.mPixelSize.mHeight});

  const auto screenDX = data->pt.x - mOrigin.x;
  const auto screenDY = data->pt.y - mOrigin.y;

  CursorEvent ev {
    .mSource = CursorSource::InGameMouse,
    .mTouchState = CursorTouchState::NearSurface,
    .mX = std::clamp(prevPoint.mX + (screenDX * scale), 0.0f, 1.0f),
    .mY = std::clamp(prevPoint.mY + (screenDY * scale), 0.0f, 1.0f),
    .mButtons = mButtons,
  };

  using Mask = CursorEvent::MouseButtonsMask;

  switch (message) {
    case WM_MOUSEWHEEL:
      ev.mWheelDeltaY = std::bit_cast<int16_t>(HIWORD(data->mouseData));
      break;
    case WM_LBUTTONDOWN:
      ev.mButtons |= std::to_underlying(Mask::Left);
      break;
    case WM_LBUTTONUP:
      ev.mButtons &= ~std::to_underlying(Mask::Left);
      break;
    case WM_MBUTTONDOWN:
      ev.mButtons |= std::to_underlying(Mask::Middle);
      break;
    case WM_MBUTTONUP:
      ev.mButtons &= ~std::to_underlying(Mask::Middle);
      break;
    case WM_RBUTTONDOWN:
      ev.mButtons |= std::to_underlying(Mask::Right);
      break;
    case WM_RBUTTONUP:
      ev.mButtons &= ~std::to_underlying(Mask::Right);
      break;
    default:
      break;
  }
  mButtons = ev.mButtons;
  if (mButtons || ev.mWheelDeltaY) {
    ev.mTouchState = CursorTouchState::TouchingSurface;
  }

  target->PostCursorEvent(ev);
}

void GlobalMouseHook::OnKeyboardMessage(
  const WPARAM message,
  KBDLLHOOKSTRUCT* const data) {
  if (message != WM_KEYUP) {
    return;
  }

  if (data->vkCode != VK_ESCAPE) {
    mEscapeTimestamps = {};
    return;
  }

  std::ranges::shift_left(mEscapeTimestamps, 1);
  QueryPerformanceCounter(&mEscapeTimestamps.back());
  const auto first = mEscapeTimestamps.front().QuadPart;
  const auto now = mEscapeTimestamps.back().QuadPart;

  if (first && ((now - first) < EscapeHatchTimeoutTicks().QuadPart)) {
    dprint.Warning("GlobalMouseHook: Escape hatch triggered");
    EscapeHatch() = true;
    Unhook();
  }
}

void GlobalMouseHook::MoveFrom(GlobalMouseHook& other) noexcept {
  if (this == std::addressof(other)) {
    return;
  }

  Unhook();

  mKneeboard = std::exchange(other.mKneeboard, nullptr);
  mMouseHook = std::exchange(other.mMouseHook, nullptr);
  mKeyboardHook = std::exchange(other.mKeyboardHook, nullptr);
  mOrigin = std::move(other.mOrigin);
  mButtons = std::move(other.mButtons);

  mEscapeTimestamps = std::move(other.mEscapeTimestamps);

  if (mMouseHook) {
    Instance() = this;
  }
}

}// namespace OpenKneeboard
