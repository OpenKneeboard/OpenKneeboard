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

#include <OpenKneeboard/scope_exit.hpp>

namespace OpenKneeboard {

DirectInputJoystickListener::DirectInputJoystickListener(
  const std::stop_token& stop,
  const winrt::com_ptr<IDirectInput8>& di,
  const std::shared_ptr<DirectInputDevice>& device)
  : DirectInputListener(stop, di, device) {
}

DirectInputJoystickListener::~DirectInputJoystickListener() = default;

std::expected<void, HRESULT> DirectInputJoystickListener::Poll() {
  decltype(mState) newState {};
  {
    const auto pollResult = this->GetState(sizeof(mState), &newState);
    if (!pollResult.has_value()) {
      return pollResult;
    }
  }
  scope_exit updateState([&]() { mState = newState; });

  auto device = this->GetDevice();
  for (uint8_t i = 0; i < sizeof(mState.rgbButtons); ++i) {
    if (mState.rgbButtons[i] != newState.rgbButtons[i]) {
      device->PostButtonStateChange(
        i, static_cast<bool>(newState.rgbButtons[i] & (1 << 7)));
    }
  }

  constexpr auto maxHats = sizeof(mState.rgdwPOV) / sizeof(mState.rgdwPOV[0]);
  for (uint8_t i = 0; i < maxHats; ++i) {
    if (mState.rgdwPOV[i] != newState.rgdwPOV[i]) {
      device->PostHatStateChange(i, mState.rgdwPOV[i], newState.rgdwPOV[i]);
    }
  }

  return {};
}

void DirectInputJoystickListener::SetDataFormat() noexcept {
  this->GetDIDevice()->SetDataFormat(&c_dfDIJoystick2);
}

void DirectInputJoystickListener::OnAcquired() noexcept {
  const auto initResult = this->GetState(sizeof(mState), &mState);
  if (!initResult.has_value()) {
    winrt::throw_hresult(initResult.error());
  }
}

}// namespace OpenKneeboard
