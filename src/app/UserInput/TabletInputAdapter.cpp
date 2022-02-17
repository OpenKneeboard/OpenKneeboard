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

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabletInputDevice.h>
#include <OpenKneeboard/WintabTablet.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <bit>

#include "KneeboardState.h"
#include "UserAction.h"

namespace OpenKneeboard {

static TabletInputAdapter* gInstance = nullptr;

TabletInputAdapter::TabletInputAdapter(
  HWND window,
  const std::shared_ptr<KneeboardState>& kneeboard/*,
  const nlohmann::json& jsonSettings*/)
  : mWindow(window), mKneeboard(kneeboard) {
  if (gInstance != nullptr) {
    throw std::logic_error("There can only be one TabletInputAdapter");
  }
  gInstance = this;

  mTablet = std::make_unique<WintabTablet>(window);
  if (!mTablet->IsValid()) {
    return;
  }

  mPreviousWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
    window,
    GWLP_WNDPROC,
    reinterpret_cast<LONG_PTR>(&TabletInputAdapter::WindowProc)));
  if (!mPreviousWndProc) {
    return;
  }

  mDevice = std::make_shared<TabletInputDevice>(
    mTablet->GetDeviceName(), mTablet->GetDeviceID());

  mFlushThread = {[=](std::stop_token stopToken) {
    const auto interval
      = std::chrono::milliseconds(1000) / TabletCursorRenderHz;
    while (!stopToken.stop_requested()) {
      if (this->mHaveUnflushedEvents) {
        this->mHaveUnflushedEvents = false;
        this->mKneeboard->evFlushEvent.EmitFromMainThread();
      }
      std::this_thread::sleep_for(interval);
    }
  }};
}

TabletInputAdapter::~TabletInputAdapter() {
  if (mPreviousWndProc) {
    SetWindowLongPtrW(
      mWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(mPreviousWndProc));
  }
  gInstance = nullptr;
}

LRESULT CALLBACK TabletInputAdapter::WindowProc(
  HWND hwnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam) {
  gInstance->ProcessTabletMessage(uMsg, wParam, lParam);
  return CallWindowProc(
    gInstance->mPreviousWndProc, hwnd, uMsg, wParam, lParam);
}

std::vector<std::shared_ptr<UserInputDevice>> TabletInputAdapter::GetDevices()
  const {
  if (!mDevice) {
    return {};
  }

  std::vector<std::shared_ptr<UserInputDevice>> out;
  out.push_back(std::static_pointer_cast<UserInputDevice>(mDevice));
  return out;
}

void TabletInputAdapter::ProcessTabletMessage(
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam) {
  if (!mTablet->ProcessMessage(uMsg, wParam, lParam)) {
    return;
  }

  const auto state = mTablet->GetState();
  if (state.tabletButtons != mTabletButtons) {
    const uint16_t changedMask = state.tabletButtons ^ mTabletButtons;
    const bool pressed = state.tabletButtons & changedMask;
    const auto buttonIndex = std::countr_zero(changedMask);
    mTabletButtons = state.tabletButtons;

    dprintf("{} expresskey {}", pressed ? "Pressed" : "Released", buttonIndex);
  }


  if (state.active) {
    // TODO: make tablet rotation configurable.
    // For now, assume tablet is rotated 90 degrees clockwise
    auto tabletLimits = mTablet->GetLimits();
    auto x = static_cast<float>(tabletLimits.y - state.y);
    auto y = static_cast<float>(state.x);
    std::swap(tabletLimits.x, tabletLimits.y);

    // Cursor events use content coordinates, but as as the content isn't at
    // the origin, we need a few transformations:

    // 1. scale to canvas size
    auto canvasSize = mKneeboard->GetCanvasSize();

    const auto scaleX = static_cast<float>(canvasSize.width) / tabletLimits.x;
    const auto scaleY = static_cast<float>(canvasSize.height) / tabletLimits.y;
    // in most cases, we use `std::min` - that would be for fitting the tablet
    // in the canvas bounds, but we want to fit the canvas in the tablet, so
    // doing the opposite
    const auto scale = std::max(scaleX, scaleY);

    x *= scale;
    y *= scale;

    // 2. translate to content origgin

    const auto contentRenderRect = mKneeboard->GetContentRenderRect();
    x -= contentRenderRect.left;
    y -= contentRenderRect.top;

    // 3. scale to content size;
    const auto contentNativeSize = mKneeboard->GetContentNativeSize();
    const auto contentScale = contentNativeSize.width
      / (contentRenderRect.right - contentRenderRect.left);
    x *= contentScale;
    y *= contentScale;

    CursorEvent event {
      .mTouchState = (state.penButtons & 1) ? CursorTouchState::TOUCHING_SURFACE
                                            : CursorTouchState::NEAR_SURFACE,
      .mX = x,
      .mY = y,
      .mPressure = static_cast<float>(state.pressure) / tabletLimits.pressure,
      .mButtons = state.penButtons,
    };

    if (
      x >= 0 && x <= contentNativeSize.width && y >= 0
      && y <= contentNativeSize.height) {
      event.mPositionState = CursorPositionState::IN_CONTENT_RECT;
    } else {
      event.mPositionState = CursorPositionState::NOT_IN_CONTENT_RECT;
    }

    mKneeboard->evCursorEvent(event);
  } else {
    mKneeboard->evCursorEvent({});
  }
  // Flush later
  mHaveUnflushedEvents = true;
}

}// namespace OpenKneeboard
