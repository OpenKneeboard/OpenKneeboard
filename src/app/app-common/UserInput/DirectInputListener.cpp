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
#include <OpenKneeboard/scope_guard.h>

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
    mDIDevice->Unacquire();
  }
}

winrt::Windows::Foundation::IAsyncAction DirectInputListener::Run(
  winrt::com_ptr<IDirectInput8> di,
  std::shared_ptr<DirectInputDevice> device,
  HANDLE completionHandle) try {
  auto cancelToken = co_await winrt::get_cancellation_token();
  cancelToken.enable_propagation();
  const scope_guard markComplete(
    [completionHandle]() { SetEvent(completionHandle); });

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
  co_return;
} catch (const winrt::hresult_canceled&) {
  dprintf("DI device Run() cancelled: {}", device->GetName());
  co_return;
}

winrt::Windows::Foundation::IAsyncAction DirectInputListener::Run() noexcept {
  this->Initialize();

  if (!(mDIDevice && mEventHandle)) {
    co_return;
  }

  auto deviceName = mDevice->GetName();
  dprintf("Starting DirectInputListener::Run() for {}", deviceName);
  const scope_guard logOnExit([deviceName]() {
    dprintf(
      "Exiting DirectInputListener::Run() for {}, with {} uncaught exceptions",
      deviceName,
      std::uncaught_exceptions());
  });

  auto cancelled = co_await winrt::get_cancellation_token();
  cancelled.callback([event = mEventHandle.get()]() { SetEvent(event); });
  while (!cancelled()) {
    co_await winrt::resume_on_signal(mEventHandle.get());
    if (cancelled()) {
      co_return;
    }

    const auto pollResult = mDIDevice->Poll();
    if (pollResult != DI_OK && pollResult != DI_NOEFFECT) {
      dprintf(
        "Abandoning DI device '{}' due to DI poll error {} ({:#08x})",
        mDevice->GetName(),
        pollResult,
        std::bit_cast<uint32_t>(pollResult));
      co_return;
    }
    try {
      this->Poll();
    } catch (const winrt::hresult_error& e) {
      dprintf(
        "Abandoning DI device '{}' due to implementation poll error {} "
        "({:#08x})",
        mDevice->GetName(),
        e.code().value,
        std::bit_cast<uint32_t>(e.code().value));
      co_return;
    }
  }
}

std::shared_ptr<DirectInputDevice> DirectInputListener::GetDevice() const {
  return mDevice;
}

winrt::com_ptr<IDirectInputDevice8W> DirectInputListener::GetDIDevice() const {
  return mDIDevice;
}

}// namespace OpenKneeboard
