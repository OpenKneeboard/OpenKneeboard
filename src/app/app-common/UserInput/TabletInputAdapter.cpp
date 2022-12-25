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
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabletInputDevice.h>
#include <OpenKneeboard/UserAction.h>
#include <OpenKneeboard/UserInputButtonBinding.h>
#include <OpenKneeboard/UserInputButtonEvent.h>
#include <OpenKneeboard/WintabTablet.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <bit>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace OpenKneeboard {

static TabletInputAdapter* gInstance = nullptr;

TabletInputAdapter::TabletInputAdapter(
  HWND window,
  KneeboardState* kneeboard,
  const TabletSettings& settings)
  : mWindow(window), mKneeboard(kneeboard) {
  if (gInstance != nullptr) {
    throw std::logic_error("There can only be one TabletInputAdapter");
  }
  gInstance = this;

  mTablet = std::make_unique<WintabTablet>(
    window, WintabTablet::Priority::AlwaysActive);
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

  auto info = mTablet->GetDeviceInfo();

  mDevice = std::make_shared<TabletInputDevice>(
    info.mDeviceName, info.mDeviceID, TabletOrientation::RotateCW90);

  AddEventListener(
    mDevice->evBindingsChangedEvent, this->evSettingsChangedEvent);
  AddEventListener(mDevice->evOrientationChangedEvent, [this]() {
    this->evSettingsChangedEvent.Emit();
  });
  AddEventListener(mDevice->evUserActionEvent, this->evUserActionEvent);

  LoadSettings(settings);
}

void TabletInputAdapter::LoadSettings(const TabletSettings& settings) {
  mInitialSettings = settings;
  if (!mDevice) {
    return;
  }

  const auto deviceID = mTablet->GetDeviceInfo().mDeviceID;
  if (settings.mDevices.contains(deviceID)) {
    auto& jsonDevice = settings.mDevices.at(deviceID);
    mDevice->SetOrientation(jsonDevice.mOrientation);
    std::vector<UserInputButtonBinding> bindings;
    for (const auto& binding: jsonDevice.mExpressKeyBindings) {
      bindings.push_back({
        mDevice,
        binding.mButtons,
        binding.mAction,
      });
    }
    mDevice->SetButtonBindings(bindings);
  } else {
    mDevice->SetButtonBindings({});
  }
  this->evSettingsChangedEvent.Emit();
}

TabletInputAdapter::~TabletInputAdapter() {
  this->RemoveAllEventListeners();
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
  if (state.auxButtons != mAuxButtons) {
    const uint16_t changedMask = state.auxButtons ^ mAuxButtons;
    const bool pressed = state.auxButtons & changedMask;
    const uint64_t buttonIndex = std::countr_zero(changedMask);
    mAuxButtons = state.auxButtons;

    mDevice->evButtonEvent.Emit({
      mDevice,
      buttonIndex,
      pressed,
    });
    return;
  }

  const auto view = mKneeboard->GetActiveViewForGlobalInput();

  if (state.active) {
    auto tabletLimits = mTablet->GetDeviceInfo();

    float x, y;
    switch (mDevice->GetOrientation()) {
      case TabletOrientation::Normal:
        x = static_cast<float>(state.x);
        y = static_cast<float>(state.y);
        break;
      case TabletOrientation::RotateCW90:
        x = static_cast<float>(tabletLimits.mMaxY - state.y);
        y = static_cast<float>(state.x);
        std::swap(tabletLimits.mMaxX, tabletLimits.mMaxY);
        break;
      case TabletOrientation::RotateCW180:
        x = static_cast<float>(tabletLimits.mMaxX - state.x);
        y = static_cast<float>(tabletLimits.mMaxY - state.y);
        break;
      case TabletOrientation::RotateCW270:
        x = static_cast<float>(state.y);
        y = static_cast<float>(tabletLimits.mMaxX - state.x);
        std::swap(tabletLimits.mMaxX, tabletLimits.mMaxY);
        break;
    }

    // Cursor events use 0..1 in canvas coordinates, so we need
    // to adapt for the aspect ratio

    // 1. scale to canvas size
    auto canvasSize = view->GetCanvasSize();

    const auto scaleX
      = static_cast<float>(canvasSize.width) / tabletLimits.mMaxX;
    const auto scaleY
      = static_cast<float>(canvasSize.height) / tabletLimits.mMaxY;
    // in most cases, we use `std::min` - that would be for fitting the tablet
    // in the canvas bounds, but we want to fit the canvas in the tablet, so
    // doing the opposite
    const auto scale = std::max(scaleX, scaleY);

    x *= scale;
    y *= scale;

    // 2. get back to 0..1
    x /= canvasSize.width;
    y /= canvasSize.height;

    CursorEvent event {
      .mTouchState = (state.penButtons & 1) ? CursorTouchState::TOUCHING_SURFACE
                                            : CursorTouchState::NEAR_SURFACE,
      .mX = std::clamp<float>(x, 0, 1),
      .mY = std::clamp<float>(y, 0, 1),
      .mPressure
      = static_cast<float>(state.pressure) / tabletLimits.mMaxPressure,
      .mButtons = state.penButtons,
    };

    view->PostCursorEvent(event);
  } else {
    view->PostCursorEvent({});
  }
}

TabletSettings TabletInputAdapter::GetSettings() const {
  if (!mDevice) {
    return mInitialSettings;
  }

  auto settings = mInitialSettings;

  const auto id = mDevice->GetID();
  auto& device = settings.mDevices[id];
  device = {
    .mID = id,
    .mName = mDevice->GetName(),
    .mExpressKeyBindings = {},
    .mOrientation = mDevice->GetOrientation(),
  };

  for (const auto& binding: mDevice->GetButtonBindings()) {
    device.mExpressKeyBindings.push_back({
      .mButtons = binding.GetButtonIDs(),
      .mAction = binding.GetAction(),
    });
  }

  return settings;
}

}// namespace OpenKneeboard
