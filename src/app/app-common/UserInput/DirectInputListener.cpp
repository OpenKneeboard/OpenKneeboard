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
#include <OpenKneeboard/DirectInputDevice.hpp>
#include <OpenKneeboard/DirectInputJoystickListener.hpp>
#include <OpenKneeboard/DirectInputKeyboardListener.hpp>
#include <OpenKneeboard/DirectInputListener.hpp>
#include <OpenKneeboard/DirectInputMouseListener.hpp>
#include <OpenKneeboard/UserInputButtonEvent.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <array>

namespace OpenKneeboard {

DirectInputListener::DirectInputListener(
  const std::stop_token& stopToken,
  const winrt::com_ptr<IDirectInput8>& di,
  const std::shared_ptr<DirectInputDevice>& device)
  : mStopToken(stopToken), mDevice(device) {
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

  mEventHandle = Win32::or_throw::CreateEvent(nullptr, false, false, nullptr);

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

task<void> DirectInputListener::Run(
  std::stop_token stopToken,
  winrt::com_ptr<IDirectInput8> di,
  std::shared_ptr<DirectInputDevice> device) {
  if ((device->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD) {
    DirectInputKeyboardListener listener {stopToken, di, device};
    co_await listener.Run();
    co_return;
  }

  if ((device->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_MOUSE) {
    DirectInputMouseListener listener {stopToken, di, device};
    co_await listener.Run();
    co_return;
  }

  DirectInputJoystickListener listener {stopToken, di, device};
  co_await listener.Run();
  co_return;
}

task<void> DirectInputListener::Run() noexcept {
  this->Initialize();

  if (!(mDIDevice && mEventHandle)) {
    co_return;
  }

  auto deviceName = mDevice->GetName();
  dprint("Starting DirectInputListener::Run() for {}", deviceName);
  // not a coroutine, also not a reference or a parameter
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
  const scope_exit logOnExit([deviceName]() {
    dprint(
      "Exiting DirectInputListener::Run() for {}, with {} uncaught exceptions",
      deviceName,
      std::uncaught_exceptions());
  });

  std::stop_callback callback(
    mStopToken, [event = mEventHandle.get()]() { SetEvent(event); });
  while (!mStopToken.stop_requested()) {
    co_await winrt::resume_on_signal(mEventHandle.get());
    if (mStopToken.stop_requested()) {
      co_return;
    }

    const auto pollResult = mDIDevice->Poll();
    if (pollResult != DI_OK && pollResult != DI_NOEFFECT) {
      dprint(
        "Abandoning DI device '{}' due to DI poll error {} ({:#08x})",
        mDevice->GetName(),
        pollResult,
        std::bit_cast<uint32_t>(pollResult));
      co_return;
    }
    try {
      this->Poll();
    } catch (const winrt::hresult_error& e) {
      dprint(
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
