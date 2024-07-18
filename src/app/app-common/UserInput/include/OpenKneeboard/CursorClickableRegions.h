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
#pragma once

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/Events.h>

#include <memory>
#include <mutex>
#include <optional>

#include <d2d1.h>

namespace OpenKneeboard {

template <class T>
concept ClickableRegion = requires(T a) {
  { a.mRect } -> std::convertible_to<D2D1_RECT_F>;
};

template <ClickableRegion Button>
class CursorClickableRegions final
  : public std::enable_shared_from_this<CursorClickableRegions<Button>> {
 public:
  using SharedPtr = typename std::shared_ptr<CursorClickableRegions<Button>>;

  static SharedPtr Create(const std::vector<Button>& buttons) {
    return SharedPtr(new CursorClickableRegions<Button>(buttons));
  }

  ~CursorClickableRegions() {
  }

  std::optional<Button> GetHoverButton() const {
    const auto keepAlive = this->shared_from_this();

    std::unique_lock lock(mMutex);
    return mHoverButton;
  }

  bool HaveHoverOrPendingClick() const {
    const auto keepAlive = this->shared_from_this();

    std::unique_lock lock(mMutex);
    return static_cast<bool>(mHoverButton) || static_cast<bool>(mPressedButton);
  }

  std::vector<Button> GetButtons() const {
    const auto keepAlive = this->shared_from_this();

    std::unique_lock lock(mMutex);
    return mButtons;
  }

  std::tuple<std::optional<Button>, std::vector<Button>> GetState() const {
    const auto keepAlive = this->shared_from_this();
    std::unique_lock lock(mMutex);
    return {mHoverButton, mButtons};
  }

  Event<EventContext, const Button&> evClicked;
  Event<EventContext> evClickedWithoutButton;

  void PostCursorEvent(EventContext ec, const CursorEvent& ev) {
    const auto keepAlive = this->shared_from_this();

    const D2D1_POINT_2F cursor {ev.mX, ev.mY};
    const EventDelay delay;
    std::unique_lock lock(mMutex);
    std::optional<Button> buttonUnderCursor;

    for (const auto& button: mButtons) {
      if (IsPointInRect(cursor, button.mRect)) {
        buttonUnderCursor = button;
        break;
      }
    }

    if (ev.mTouchState == CursorTouchState::NearSurface) {
      mHoverButton = buttonUnderCursor;
    } else if (ev.mTouchState == CursorTouchState::NotNearSurface) {
      mHoverButton.reset();
    }

    if (
      mCursorTouching && ev.mTouchState == CursorTouchState::TouchingSurface) {
      return;
    }

    if (
      (!mCursorTouching)
      && ev.mTouchState != CursorTouchState::TouchingSurface) {
      return;
    }

    mCursorTouching = (ev.mTouchState == CursorTouchState::TouchingSurface);

    if (mCursorTouching) {
      // Touch start
      mPressedButton = buttonUnderCursor;
      return;
    }

    // Touch end
    if (!mPressedButton) {
      if (!buttonUnderCursor) {
        evClickedWithoutButton.Emit(ec);
      }
      return;
    }
    const auto pressedButton = *mPressedButton;
    mPressedButton = {};

    if (buttonUnderCursor != pressedButton) {
      return;
    }

    evClicked.Emit(ec, pressedButton);
  }

  CursorClickableRegions() = delete;

 private:
  bool mCursorTouching = false;
  std::vector<Button> mButtons;
  std::optional<Button> mHoverButton;
  std::optional<Button> mPressedButton;
  mutable std::mutex mMutex;

  constexpr bool IsPointInRect(const D2D1_POINT_2F& p, const D2D1_RECT_F& r) {
    return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
  };

  CursorClickableRegions(const std::vector<Button>& buttons)
    : mButtons(buttons) {
  }
};

}// namespace OpenKneeboard
