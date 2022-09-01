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
#include <OpenKneeboard/DirectInputJoystickListener.h>
#include <OpenKneeboard/DirectInputKeyboardListener.h>
#include <OpenKneeboard/DirectInputListener.h>
#include <OpenKneeboard/DirectInputMouseListener.h>
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
}

void DirectInputListener::Initialize() {
  if (mInitialized || !mDIDevice) {
    return;
  }
  mInitialized = true;

  mEventHandle.attach(CreateEvent(nullptr, false, false, nullptr));
  if (!mEventHandle) {
    return;
  }

  this->SetDataFormat();

  winrt::check_hresult(mDIDevice->SetEventNotification(mEventHandle.get()));
  winrt::check_hresult(mDIDevice->SetCooperativeLevel(
    NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE));
  winrt::check_hresult(mDIDevice->Acquire());

  this->OnAcquired();
}

DirectInputListener::~DirectInputListener() {
  if (mDIDevice) {
    // Must detach the event handle before winrt::handle destroys it
    mDIDevice->SetEventNotification(NULL);
  }
}

winrt::Windows::Foundation::IAsyncAction DirectInputListener::Run(
  const winrt::com_ptr<IDirectInput8>& di,
  const std::shared_ptr<DirectInputDevice>& device) noexcept {
  if ((device->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD) {
    DirectInputKeyboardListener listener {di, device};
    co_await listener.Run();
    co_return;
  }

  if ((device->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_MOUSE) {
    DirectInputMouseListener listener {di, device};
    co_await listener.Run();
    co_return;
  }

  DirectInputJoystickListener listener {di, device};
  co_await listener.Run();
}

winrt::Windows::Foundation::IAsyncAction DirectInputListener::Run() noexcept {
  this->Initialize();

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

    const auto pollResult = mDIDevice->Poll();
    if (pollResult != DI_OK && pollResult != DI_NOEFFECT) {
      dprintf(
        "Abandoning DI device '{}' due to error {} ({:#08x})",
        mDevice->GetName(),
        pollResult,
        std::bit_cast<uint32_t>(pollResult));
      co_return;
    }
    this->Poll();
  }
}

std::shared_ptr<DirectInputDevice> DirectInputListener::GetDevice() const {
  return mDevice;
}

winrt::com_ptr<IDirectInputDevice8W> DirectInputListener::GetDIDevice() const {
  return mDIDevice;
}

}// namespace OpenKneeboard
