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
#include <OpenKneeboard/OTDIPCClient.h>
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabletInputDevice.h>
#include <OpenKneeboard/UserAction.h>
#include <OpenKneeboard/UserInputButtonBinding.h>
#include <OpenKneeboard/UserInputButtonEvent.h>
#include <OpenKneeboard/WintabTablet.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/handles.h>

#include <atomic>
#include <bit>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace OpenKneeboard {

static std::atomic_flag gHaveInstance;
static std::weak_ptr<TabletInputAdapter> gInstance;

std::shared_ptr<TabletInputAdapter> TabletInputAdapter::Create(
  HWND hwnd,
  KneeboardState* kbs,
  const TabletSettings& tablet) {
  if (gHaveInstance.test_and_set()) {
    throw std::logic_error("There can only be one TabletInputAdapter");
  }
  auto ret = std::shared_ptr<TabletInputAdapter>(
    new TabletInputAdapter(hwnd, kbs, tablet));

  gInstance = ret;
  ret->Init();

  return ret;
}

TabletInputAdapter::TabletInputAdapter(
  HWND window,
  KneeboardState* kneeboard,
  const TabletSettings& settings)
  : mWindow(window), mKneeboard(kneeboard) {
  LoadSettings(settings);
}

void TabletInputAdapter::Init() {
  this->StartWintab();
  this->StartOTDIPC();
}

bool TabletInputAdapter::IsOTDIPCEnabled() const {
  return mSettings.mOTDIPC;
}

void TabletInputAdapter::SetIsOTDIPCEnabled(bool value) {
  if (value == IsOTDIPCEnabled()) {
    return;
  }

  mSettings.mOTDIPC = value;
  if (value) {
    StartOTDIPC();
  } else {
    StopOTDIPC();
  }
  evSettingsChangedEvent.Emit();
}

void TabletInputAdapter::StartOTDIPC() {
  if (mOTDIPC) {
    return;
  }
  if (!mSettings.mOTDIPC) {
    return;
  }

  mOTDIPC = OTDIPCClient::Create();
  AddEventListener(
    mOTDIPC->evTabletInputEvent,
    std::bind_front(&TabletInputAdapter::OnOTDInput, this));
}

void TabletInputAdapter::StopOTDIPC() {
  mOTDIPC.reset();
}

WintabMode TabletInputAdapter::GetWintabMode() const {
  return mSettings.mWintab;
}

winrt::Windows::Foundation::IAsyncAction TabletInputAdapter::SetWintabMode(
  WintabMode mode) {
  if (mode == GetWintabMode()) {
    co_return;
  }

  if (mode == WintabMode::Disabled) {
    mSettings.mWintab = mode;
    StopWintab();
    evSettingsChangedEvent.Emit();
    co_return;
  }

  // Check that we can actually load Wintab before we save it; some drivers
  // - especially XP-Pen - will crash as soon as they're loaded
  if (!mWintabTablet) {
    dprint("Attempting to load wintab");
    unique_hmodule wintab {LoadLibraryW(L"WINTAB32.dll")};
    co_await winrt::resume_after(std::chrono::milliseconds(100));
    dprint("Loaded wintab!");
  }

  mSettings.mWintab = mode;
  StartWintab();
  if (!mWintabTablet) {
    dprint("Failed to initialize wintab");
    co_return;
  }
  auto priority = (mSettings.mWintab == WintabMode::Enabled)
    ? WintabTablet::Priority::AlwaysActive
    : WintabTablet::Priority::ForegroundOnly;
  mWintabTablet->SetPriority(priority);
  // Again, make sure that doesn't crash :)
  co_await winrt::resume_after(std::chrono::milliseconds(100));
  evSettingsChangedEvent.Emit();
}

void TabletInputAdapter::StartWintab() {
  if (mSettings.mWintab == WintabMode::Disabled) {
    return;
  }

  if (!mWintabTablet) {
    // If mode is 'Invasive', we manage background access by
    // injecting a DLL, so as far as our wintab is concerned,
    // it's only dealing with the foreground
    auto priority = (mSettings.mWintab == WintabMode::Enabled)
      ? WintabTablet::Priority::AlwaysActive
      : WintabTablet::Priority::ForegroundOnly;
    mWintabTablet = std::make_unique<WintabTablet>(mWindow, priority);
    if (!mWintabTablet->IsValid()) {
      mWintabTablet.reset();
      return;
    }
  }

  if (mPreviousWndProc) {
    return;
  }

  mPreviousWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
    mWindow,
    GWLP_WNDPROC,
    reinterpret_cast<LONG_PTR>(&TabletInputAdapter::WindowProc_Wintab)));
  if (!mPreviousWndProc) {
    return;
  }

  auto info = mWintabTablet->GetDeviceInfo();
  mWintabDevice = this->CreateDevice(info.mDeviceName, info.mDeviceID);
}

std::shared_ptr<TabletInputDevice> TabletInputAdapter::CreateDevice(
  const std::string& name,
  const std::string& id) {
  auto device = std::make_shared<TabletInputDevice>(
    name, id, TabletOrientation::RotateCW90);
  LoadSettings(mSettings, device);

  AddEventListener(
    device->evBindingsChangedEvent, this->evSettingsChangedEvent);
  AddEventListener(device->evOrientationChangedEvent, [this]() {
    this->evSettingsChangedEvent.Emit();
  });
  AddEventListener(device->evUserActionEvent, this->evUserActionEvent);

  return device;
}

void TabletInputAdapter::LoadSettings(const TabletSettings& settings) {
  mSettings = settings;

  for (auto& device: this->GetDevices()) {
    auto tabletDevice = std::static_pointer_cast<TabletInputDevice>(device);
    LoadSettings(settings, tabletDevice);
  }
  this->evSettingsChangedEvent.Emit();
}

void TabletInputAdapter::LoadSettings(
  const TabletSettings& settings,
  const std::shared_ptr<TabletInputDevice>& tablet) {
  const auto deviceID = tablet->GetID();
  if (!settings.mDevices.contains(deviceID)) {
    tablet->SetButtonBindings({});
    return;
  }

  auto& jsonDevice = settings.mDevices.at(deviceID);
  tablet->SetOrientation(jsonDevice.mOrientation);
  std::vector<UserInputButtonBinding> bindings;
  for (const auto& binding: jsonDevice.mExpressKeyBindings) {
    bindings.push_back({
      tablet,
      binding.mButtons,
      binding.mAction,
    });
  }
  tablet->SetButtonBindings(bindings);
}

TabletInputAdapter::~TabletInputAdapter() {
  StopWintab();
  StopOTDIPC();
  this->RemoveAllEventListeners();
  gHaveInstance.clear();
}

void TabletInputAdapter::StopWintab() {
  if (!mWintabTablet) {
    return;
  }
  if (!mPreviousWndProc) {
    return;
  }
  SetWindowLongPtrW(
    mWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(mPreviousWndProc));
  mWintabTablet = {};
  mPreviousWndProc = {};
}

LRESULT CALLBACK TabletInputAdapter::WindowProc_Wintab(
  HWND hwnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam) {
  auto instance = gInstance.lock();
  if (!instance) {
    OPENKNEEBOARD_BREAK;
    return 0;
  }

  instance->OnWintabMessage(uMsg, wParam, lParam);
  return CallWindowProc(instance->mPreviousWndProc, hwnd, uMsg, wParam, lParam);
}

std::vector<std::shared_ptr<UserInputDevice>> TabletInputAdapter::GetDevices()
  const {
  std::vector<std::shared_ptr<UserInputDevice>> ret;

  if (mWintabDevice) {
    ret.push_back(std::static_pointer_cast<UserInputDevice>(mWintabDevice));
  }

  for (const auto& [_, device]: mOTDDevices) {
    ret.push_back(device);
  }

  return ret;
}

void TabletInputAdapter::OnWintabMessage(
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam) {
  if (!mWintabTablet->ProcessMessage(uMsg, wParam, lParam)) {
    return;
  }
  const auto tablet = mWintabTablet->GetDeviceInfo();
  const auto state = mWintabTablet->GetState();
  this->OnTabletInput(tablet, state, mWintabDevice);
}

void TabletInputAdapter::OnTabletInput(
  const TabletInfo& tablet,
  const TabletState& state,
  const std::shared_ptr<TabletInputDevice>& device) {
  auto& auxButtons = mAuxButtons[tablet.mDeviceID];

  if (state.mAuxButtons != auxButtons) {
    const uint32_t changedMask = state.mAuxButtons ^ auxButtons;
    const bool isPressed = state.mAuxButtons & changedMask;
    const uint64_t buttonIndex = std::countr_zero(changedMask);
    auxButtons = state.mAuxButtons;

    device->evButtonEvent.Emit({
      device,
      buttonIndex,
      isPressed,
    });
    return;
  }

  const auto view = mKneeboard->GetActiveViewForGlobalInput();

  auto maxX = tablet.mMaxX;
  auto maxY = tablet.mMaxY;

  if (state.mIsActive) {
    float x, y;
    switch (device->GetOrientation()) {
      case TabletOrientation::Normal:
        x = state.mX;
        y = state.mY;
        break;
      case TabletOrientation::RotateCW90:
        x = maxY - state.mY;
        y = state.mX;
        std::swap(maxX, maxY);
        break;
      case TabletOrientation::RotateCW180:
        x = maxX - state.mX;
        y = maxY - state.mY;
        break;
      case TabletOrientation::RotateCW270:
        x = state.mY;
        y = maxX - state.mX;
        std::swap(maxX, maxY);
        break;
    }

    // Cursor events use 0..1 in canvas coordinates, so we need
    // to adapt for the aspect ratio

    // 1. scale to canvas size
    auto canvasSize = view->GetCanvasSize();

    const auto scaleX = canvasSize.width / maxX;
    const auto scaleY = canvasSize.height / maxY;
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
      .mTouchState = (state.mPenButtons & 1)
        ? CursorTouchState::TOUCHING_SURFACE
        : CursorTouchState::NEAR_SURFACE,
      .mX = std::clamp<float>(x, 0, 1),
      .mY = std::clamp<float>(y, 0, 1),
      .mPressure = static_cast<float>(state.mPressure) / tablet.mMaxPressure,
      .mButtons = state.mPenButtons,
    };

    view->PostCursorEvent(event);
  } else {
    view->PostCursorEvent({});
  }
}

TabletSettings TabletInputAdapter::GetSettings() const {
  auto settings = mSettings;

  for (auto& device: this->GetDevices()) {
    auto tablet = std::static_pointer_cast<TabletInputDevice>(device);
    const auto id = device->GetID();
    auto& deviceSettings = settings.mDevices[id];
    deviceSettings = {
      .mID = id,
      .mName = device->GetName(),
      .mExpressKeyBindings = {},
      .mOrientation = tablet->GetOrientation(),
    };

    for (const auto& binding: device->GetButtonBindings()) {
      deviceSettings.mExpressKeyBindings.push_back({
        .mButtons = binding.GetButtonIDs(),
        .mAction = binding.GetAction(),
      });
    }
  }

  return settings;
}

std::shared_ptr<TabletInputDevice> TabletInputAdapter::GetOTDDevice(
  const std::string& id) {
  auto it = mOTDDevices.find(id);
  if (it != mOTDDevices.end()) {
    return it->second;
  }

  auto info = mOTDIPC->GetTablet(id);
  if (!info) {
    return nullptr;
  }

  auto device = CreateDevice(info->mDeviceName, info->mDeviceID);
  mOTDDevices[info->mDeviceID] = device;
  evDeviceConnectedEvent.Emit(device);
  return device;
}

void TabletInputAdapter::OnOTDInput(
  const std::string& id,
  const TabletState& state) {
  auto tablet = mOTDIPC->GetTablet(id);
  if (!tablet) {
    dprint("Received OTD input without device info");
    OPENKNEEBOARD_BREAK;
    return;
  }

  auto device = GetOTDDevice(id);
  if (!device) {
    dprint("Received OTD input but couldn't create a TabletInputDevice");
    OPENKNEEBOARD_BREAK;
    return;
  }

  this->OnTabletInput(*tablet, state, device);
}

}// namespace OpenKneeboard
