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

winrt::Windows::Foundation::IAsyncAction DirectInputListener::Run() {
  std::vector<winrt::Windows::Foundation::IAsyncAction> subs;
  for (auto& device: mDevices) {
    subs.push_back(Run(device));
  }
  auto cancelled = co_await winrt::get_cancellation_token();
  cancelled.callback([&]() {
    for (auto& sub: subs) {
      sub.Cancel();
    }
  });
  for (auto& sub: subs) {
    co_await sub;
  }
}

winrt::Windows::Foundation::IAsyncAction DirectInputListener::Run(
  DeviceInfo& device) {
  auto cancelled = co_await winrt::get_cancellation_token();
  cancelled.callback([&device]() {
    /** SPAMMM */
    SetEvent(device.mEventHandle);
  });
  while (!cancelled()) {
    co_await winrt::resume_on_signal(device.mEventHandle);
    if (cancelled()) {
      co_return;
    }

    auto oldState = device.mState;
    decltype(oldState) newState {};
    const auto pollResult = device.mDIDevice->Poll();
    if (pollResult != DI_OK && pollResult != DI_NOEFFECT) {
      dprintf(
        "Abandoning DI device '{}' due to error {} ({:#08x})",
        device.mDevice->GetName(),
        pollResult,
        std::bit_cast<uint32_t>(pollResult));
      co_return;
    }
    if (
      (device.mDevice->GetDIDeviceInstance().dwDevType & 0xff)
      == DI8DEVTYPE_KEYBOARD) {
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
        device.mDevice->evButtonEvent.Emit({
          device.mDevice,
          i,
          static_cast<bool>(newState[i] & (1 << 7)),
        });
      }
    }
  }
}

}// namespace OpenKneeboard
