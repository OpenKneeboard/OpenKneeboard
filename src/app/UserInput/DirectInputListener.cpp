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
#include <OpenKneeboard/DirectInputDevice.h>
#include <OpenKneeboard/DirectInputListener.h>
#include <OpenKneeboard/UserInputButtonEvent.h>
#include <shims/wx.h>

namespace OpenKneeboard {

struct DirectInputListener::DeviceInfo {
  std::shared_ptr<DirectInputDevice> mDevice;
  winrt::com_ptr<IDirectInputDevice8> mDIDevice;
  DIJOYSTATE2 mState = {};
  HANDLE mEventHandle = INVALID_HANDLE_VALUE;
};

DirectInputListener::DirectInputListener(
  const winrt::com_ptr<IDirectInput8>& di,
  const std::vector<std::shared_ptr<DirectInputDevice>>& devices) {
  for (const auto& it: devices) {
    winrt::com_ptr<IDirectInputDevice8> device;
    di->CreateDevice(
      it->GetDIDeviceInstance().guidInstance, device.put(), NULL);
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

DirectInputListener::~DirectInputListener() {
}

void DirectInputListener::Run(std::stop_token stopToken) {
  winrt::handle cancelHandle {CreateEvent(nullptr, false, false, nullptr)};

  std::stop_callback stopEvent(
    stopToken, [&]() { SetEvent(cancelHandle.get()); });

  std::vector<HANDLE> handles;
  for (const auto& it: mDevices) {
    handles.push_back(it.mEventHandle);
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
    auto oldState = device.mState;
    DIJOYSTATE2 newState;
    device.mDIDevice->Poll();
    device.mDIDevice->GetDeviceState(sizeof(newState), &newState);
    if (memcmp(oldState.rgbButtons, newState.rgbButtons, 128) == 0) {
      continue;
    }

    for (uint8_t i = 0; i < 128; ++i) {
      if (oldState.rgbButtons[i] != newState.rgbButtons[i]) {
        device.mState = newState;
        device.mDevice->evButtonEvent.EmitFromMainThread({
          device.mDevice,
          static_cast<uint64_t>(i),
          static_cast<bool>(newState.rgbButtons[i] & (1 << 7)),
        });
      }
    }
  }
}

}// namespace OpenKneeboard
