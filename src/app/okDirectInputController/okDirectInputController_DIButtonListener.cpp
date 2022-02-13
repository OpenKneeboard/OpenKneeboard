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
#include "okDirectInputController_DIButtonListener.h"

#include "okDirectInputController_DIButtonEvent.h"

struct okDirectInputController::DIButtonListener::DeviceInfo {
  DIDEVICEINSTANCE instance;
  winrt::com_ptr<IDirectInputDevice8> device;
  DIJOYSTATE2 state = {};
  HANDLE eventHandle = INVALID_HANDLE_VALUE;
};

okDirectInputController::DIButtonListener::DIButtonListener(
  const winrt::com_ptr<IDirectInput8>& di,
  const std::vector<DIDEVICEINSTANCE>& instances) {
  for (const auto& it: instances) {
  winrt::com_ptr<IDirectInputDevice8> device;
  di->CreateDevice(it.guidInstance, device.put(), NULL);
  if (!device) {
    continue;
  }

  HANDLE event {CreateEvent(nullptr, false, false, nullptr)};
  if (!event) {
    continue;
  }

  device->SetDataFormat(&c_dfDIJoystick2);
  device->SetEventNotification(event);
  device->SetCooperativeLevel(
    static_cast<wxApp*>(wxApp::GetInstance())->GetTopWindow()->GetHandle(),
    DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
  device->Acquire();

  DIJOYSTATE2 state;
  device->GetDeviceState(sizeof(state), &state);

  mDevices.push_back({it, device, state, event});
  }
}

okDirectInputController::DIButtonListener::~DIButtonListener() {
}

void okDirectInputController::DIButtonListener::Run(std::stop_token stopToken) {
  winrt::handle cancelHandle {CreateEvent(nullptr, false, false, nullptr)};

  std::stop_callback stopEvent(
    stopToken, [&]() { SetEvent(cancelHandle.get()); });

  std::vector<HANDLE> handles;
  for (const auto& it: mDevices) {
    handles.push_back(it.eventHandle);
  }
  handles.push_back(cancelHandle.get());

  while (!stopToken.stop_requested()) {
    auto result
      = WaitForMultipleObjects(handles.size(), handles.data(), false, INFINITE);
    auto idx = result - WAIT_OBJECT_0;
    if (idx < 0 || idx >= (handles.size() - 1)) {
      continue;
    }

    auto& device = mDevices.at(idx);
    auto oldState = device.state;
    DIJOYSTATE2 newState;
    device.device->Poll();
    device.device->GetDeviceState(sizeof(newState), &newState);
    if (memcmp(oldState.rgbButtons, newState.rgbButtons, 128) == 0) {
      continue;
    }

    for (uint8_t i = 0; i < 128; ++i) {
      if (oldState.rgbButtons[i] != newState.rgbButtons[i]) {
        device.state = newState;
        evButtonEvent.EmitFromMainThread({
          device.instance,
          static_cast<uint8_t>(i),
          (bool)(newState.rgbButtons[i] & (1 << 7)),
        });
      }
    }
  }
}
