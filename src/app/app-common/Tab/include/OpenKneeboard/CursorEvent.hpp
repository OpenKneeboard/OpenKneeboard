// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <cstdint>
#include <utility>

namespace OpenKneeboard {

enum class CursorTouchState {
  TouchingSurface,
  NearSurface,
  NotNearSurface,
};

enum class CursorSource {
  WindowPointer,
  Tablet,
};

struct CursorEvent {
  enum class MouseButtonsMask : uint32_t {
    Left = 1 << 0,
    Right = 1 << 1,
    Middle = 1 << 2,
    XButton1 = 1 << 3,
    XButton2 = 1 << 4,
  };
  // 1 click of a standard mouse wheel; mZ will *usually* be a multiple of this,
  // but some mice may support smooth scrolling
  static constexpr auto MouseWheelDelta = 120;

  CursorSource mSource = CursorSource::Tablet;
  CursorTouchState mTouchState = CursorTouchState::NotNearSurface;
  float mX = 0, mY = 0, mPressure = 0;
  uint32_t mButtons = 0;
  int16_t mWheelDeltaY = 0;

  [[nodiscard]]
  constexpr bool HaveMouseData(const MouseButtonsMask mask) const noexcept {
    if (mSource != CursorSource::WindowPointer) {
      return false;
    }

    const auto bits = std::to_underlying(mask);
    return (mButtons & bits) == bits;
  }

  [[nodiscard]]
  constexpr bool HavePrimaryButton() const noexcept {
    return (mButtons & 1);
  }

  [[nodiscard]]
  constexpr bool HaveSecondaryButton() const noexcept {
    if (mSource == CursorSource::WindowPointer) {
      return HaveMouseData(MouseButtonsMask::Right);
    }
    return (mButtons & ~1);// Not a left click
  }
};

}// namespace OpenKneeboard
