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
#include <OpenKneeboard/dprint.h>
#include <shims/wx.h>

#include <array>

namespace OpenKneeboard {

struct DirectInputListener::DeviceInfo {
  std::shared_ptr<DirectInputDevice> mDevice;
  winrt::com_ptr<IDirectInputDevice8> mDIDevice;
  HANDLE mEventHandle = INVALID_HANDLE_VALUE;
  std::array<unsigned char, 256> mState;
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

    std::array<unsigned char, 256> state {};

    if ((it->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD) {
      winrt::check_hresult(device->SetDataFormat(&c_dfDIKeyboard));
    } else {
      winrt::check_hresult(device->SetDataFormat(&c_dfDIJoystick2));
    }

    winrt::check_hresult(device->SetEventNotification(event));
    winrt::check_hresult(
      device->SetCooperativeLevel(NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE));
    winrt::check_hresult(device->Acquire());

    if ((it->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD) {
      winrt::check_hresult(device->GetDeviceState(state.size(), state.data()));
    } else {
      DIJOYSTATE2 joyState;
      winrt::check_hresult(device->GetDeviceState(sizeof(joyState), &joyState));
      ::memcpy(state.data(), joyState.rgbButtons, 128);
    }
    mDevices.push_back({
      .mDevice = it,
      .mDIDevice = device,
      .mEventHandle = event,
      .mState = state,
    });
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
    decltype(oldState) newState {};
    device.mDIDevice->Poll();
    if ((device.mDevice->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD) {
      device.mDIDevice->GetDeviceState(sizeof(newState), &newState);
    } else {
      DIJOYSTATE2 joyState;
      device.mDIDevice->GetDeviceState(sizeof(joyState), &joyState);
      ::memcpy(newState.data(), joyState.rgbButtons, 128);
    }
    if (::memcmp(oldState.data(), newState.data(), sizeof(newState)) == 0) {
      continue;
    }

    device.mState = newState;

    for (uint64_t i = 0; i < newState.size(); ++i) {
      if (oldState[i] != newState[i]) {
        device.mDevice->evButtonEvent.EmitFromMainThread({
          device.mDevice,
          i,
          static_cast<bool>(newState[i] & (1 << 7)),
        });
      }
    }
  }
}

}// namespace OpenKneeboard
