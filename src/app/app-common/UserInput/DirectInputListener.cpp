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

DirectInputListener::DirectInputListener(
  const winrt::com_ptr<IDirectInput8>& di,
  const std::shared_ptr<DirectInputDevice>& device)
  : mDevice(device) {
  di->CreateDevice(
    device->GetDIDeviceInstance().guidInstance, mDIDevice.put(), NULL);
  if (!mDIDevice) {
    return;
  }

  mEventHandle.attach(CreateEvent(nullptr, false, false, nullptr));
  if (!mEventHandle) {
    return;
  }

  if ((device->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD) {
    winrt::check_hresult(mDIDevice->SetDataFormat(&c_dfDIKeyboard));
  } else {
    winrt::check_hresult(mDIDevice->SetDataFormat(&c_dfDIJoystick2));
  }

  winrt::check_hresult(mDIDevice->SetEventNotification(mEventHandle.get()));
  winrt::check_hresult(mDIDevice->SetCooperativeLevel(
    NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE));
  winrt::check_hresult(mDIDevice->Acquire());

  if ((device->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD) {
    winrt::check_hresult(
      mDIDevice->GetDeviceState(mState.size(), mState.data()));
  } else {
    DIJOYSTATE2 joyState;
    winrt::check_hresult(
      mDIDevice->GetDeviceState(sizeof(joyState), &joyState));
    ::memcpy(mState.data(), joyState.rgbButtons, 128);
  }
}

DirectInputListener::~DirectInputListener() {
  if (mDIDevice) {
    // Must detach the event handle before winrt::handle destroys it
    mDIDevice->SetEventNotification(NULL);
  }
}

winrt::Windows::Foundation::IAsyncAction DirectInputListener::Run(
  const winrt::com_ptr<IDirectInput8>& di,
  const std::shared_ptr<DirectInputDevice>& device) {
  DirectInputListener listener {di, device};
  co_await listener.Run();
}

winrt::Windows::Foundation::IAsyncAction DirectInputListener::Run() {
  if (!(mDIDevice && mEventHandle)) {
    co_return;
  }
  auto cancelled = co_await winrt::get_cancellation_token();
  cancelled.callback([this]() {
    /** SPAMMM */
    SetEvent(mEventHandle.get());
  });
  while (!cancelled()) {
    co_await winrt::resume_on_signal(mEventHandle.get());
    if (cancelled()) {
      co_return;
    }

    auto oldState = mState;
    decltype(oldState) newState {};
    const auto pollResult = mDIDevice->Poll();
    if (pollResult != DI_OK && pollResult != DI_NOEFFECT) {
      dprintf(
        "Abandoning DI device '{}' due to error {} ({:#08x})",
        mDevice->GetName(),
        pollResult,
        std::bit_cast<uint32_t>(pollResult));
      co_return;
    }
    if (
      (mDevice->GetDIDeviceInstance().dwDevType & 0xff)
      == DI8DEVTYPE_KEYBOARD) {
      mDIDevice->GetDeviceState(sizeof(newState), &newState);
    } else {
      DIJOYSTATE2 joyState;
      mDIDevice->GetDeviceState(sizeof(joyState), &joyState);
      ::memcpy(newState.data(), joyState.rgbButtons, 128);
    }
    if (::memcmp(oldState.data(), newState.data(), sizeof(newState)) == 0) {
      continue;
    }

    mState = newState;

    for (uint8_t i = 0; i < newState.size(); ++i) {
      if (oldState[i] != newState[i]) {
        mDevice->PostButtonStateChange(
          i, static_cast<bool>(newState[i] & (1 << 7)));
      }
    }
  }
}

}// namespace OpenKneeboard
