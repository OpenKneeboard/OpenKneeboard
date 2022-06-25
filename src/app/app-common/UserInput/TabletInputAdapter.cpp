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

namespace {

TabletInputAdapter* gInstance = nullptr;

struct JSONButtonBinding {
  std::unordered_set<uint64_t> Buttons;
  UserAction Action;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JSONButtonBinding, Buttons, Action);
struct JSONDevice {
  std::string ID;
  std::string Name;
  std::vector<JSONButtonBinding> ExpressKeyBindings;
  TabletOrientation Orientation {TabletOrientation::RotateCW90};
};

void to_json(nlohmann::json& j, const JSONDevice& device) {
  j = {
    {"ID", device.ID},
    {"Name", device.Name},
    {"ExpressKeyBindings", device.ExpressKeyBindings},
    {"Orientation", device.Orientation},
  };
}

void from_json(const nlohmann::json& j, JSONDevice& device) {
  device.ID = j.at("ID");
  device.Name = j.at("Name");
  device.ExpressKeyBindings = j.at("ExpressKeyBindings");

  if (j.contains("Orientation")) {
    device.Orientation = j.at("Orientation");
  }
}

struct JSONSettings {
  std::unordered_map<std::string, JSONDevice> Devices;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JSONSettings, Devices);

}// namespace

TabletInputAdapter::TabletInputAdapter(
  HWND window,
  KneeboardState* kneeboard,
  const nlohmann::json& jsonSettings)
  : mWindow(window), mKneeboard(kneeboard), mInitialSettings(jsonSettings) {
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
    mTablet->GetDeviceName(),
    mTablet->GetDeviceID(),
    TabletOrientation::RotateCW90);

  JSONSettings settings;
  if (!jsonSettings.is_null()) {
    jsonSettings.get_to(settings);
  }

  if (settings.Devices.contains(mTablet->GetDeviceID())) {
    auto& jsonDevice = settings.Devices.at(mTablet->GetDeviceID());
    mDevice->SetOrientation(jsonDevice.Orientation);
    std::vector<UserInputButtonBinding> bindings;
    for (const auto& binding: jsonDevice.ExpressKeyBindings) {
      bindings.push_back({
        mDevice,
        binding.Buttons,
        binding.Action,
      });
    }
    mDevice->SetButtonBindings(bindings);
  }

  AddEventListener(
    mDevice->evBindingsChangedEvent, this->evSettingsChangedEvent);
  AddEventListener(mDevice->evOrientationChangedEvent, [this]() {
    this->evSettingsChangedEvent.Emit();
  });
  AddEventListener(mDevice->evUserActionEvent, this->evUserActionEvent);
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
  if (state.tabletButtons != mTabletButtons) {
    const uint16_t changedMask = state.tabletButtons ^ mTabletButtons;
    const bool pressed = state.tabletButtons & changedMask;
    const uint64_t buttonIndex = std::countr_zero(changedMask);
    mTabletButtons = state.tabletButtons;

    mDevice->evButtonEvent.Emit({
      mDevice,
      buttonIndex,
      pressed,
    });
    return;
  }

  const auto view = mKneeboard->GetActiveViewForGlobalInput();

  if (state.active) {
    auto tabletLimits = mTablet->GetLimits();

    float x, y;
    switch (mDevice->GetOrientation()) {
      case TabletOrientation::Normal:
        x = static_cast<float>(state.x);
        y = static_cast<float>(state.y);
        break;
      case TabletOrientation::RotateCW90:
        x = static_cast<float>(tabletLimits.y - state.y);
        y = static_cast<float>(state.x);
        std::swap(tabletLimits.x, tabletLimits.y);
        break;
      case TabletOrientation::RotateCW180:
        x = static_cast<float>(tabletLimits.x - state.x);
        y = static_cast<float>(tabletLimits.y - state.y);
        break;
      case TabletOrientation::RotateCW270:
        x = static_cast<float>(state.y);
        y = static_cast<float>(tabletLimits.x - state.x);
        std::swap(tabletLimits.x, tabletLimits.y);
        break;
    }

    // Cursor events use content coordinates, but as as the content isn't at
    // the origin, we need a few transformations:

    // 1. scale to canvas size
    auto canvasSize = view->GetCanvasSize();

    const auto scaleX = static_cast<float>(canvasSize.width) / tabletLimits.x;
    const auto scaleY = static_cast<float>(canvasSize.height) / tabletLimits.y;
    // in most cases, we use `std::min` - that would be for fitting the tablet
    // in the canvas bounds, but we want to fit the canvas in the tablet, so
    // doing the opposite
    const auto scale = std::max(scaleX, scaleY);

    x *= scale;
    y *= scale;

    // 2. translate to content origgin

    const auto contentRenderRect = view->GetContentRenderRect();
    x -= contentRenderRect.left;
    y -= contentRenderRect.top;

    // 3. scale to content size;
    const auto contentNativeSize = view->GetContentNativeSize();
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

    view->PostCursorEvent(event);
  } else {
    view->PostCursorEvent({});
  }
}

nlohmann::json TabletInputAdapter::GetSettings() const {
  if (!mDevice) {
    return mInitialSettings;
  }

  JSONSettings settings;
  if (!mInitialSettings.is_null()) {
    mInitialSettings.get_to(settings);
  }

  const auto id = mDevice->GetID();
  if (settings.Devices.contains(id)) {
    settings.Devices.erase(id);
  }

  std::vector<JSONButtonBinding> expressKeyBindings;
  for (const auto& binding: mDevice->GetButtonBindings()) {
    expressKeyBindings.push_back({
      .Buttons = binding.GetButtonIDs(),
      .Action = binding.GetAction(),
    });
  }
  settings.Devices[id] = {
    .ID = id,
    .Name = mDevice->GetName(),
    .ExpressKeyBindings = expressKeyBindings,
    .Orientation = mDevice->GetOrientation(),
  };

  return settings;
}

}// namespace OpenKneeboard
