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
#include <OpenKneeboard/DirectInputMouseListener.hpp>

#include <OpenKneeboard/scope_exit.hpp>

namespace OpenKneeboard {

DirectInputMouseListener::DirectInputMouseListener(
  const std::stop_token& stop,
  const winrt::com_ptr<IDirectInput8>& di,
  const std::shared_ptr<DirectInputDevice>& device)
  : DirectInputListener(stop, di, device) {
}

DirectInputMouseListener::~DirectInputMouseListener() = default;

std::expected<void, HRESULT> DirectInputMouseListener::Poll() {
  decltype(mState) newState {};
  {
    const auto pollResult = this->GetState(sizeof(mState), &newState);
    if (!pollResult.has_value()) {
      return pollResult;
    }
  }
  scope_exit updateState([&]() { mState = newState; });

  auto device = this->GetDevice();
  for (size_t i = 0; i < sizeof(mState.rgbButtons); ++i) {
    if (mState.rgbButtons[i] != newState.rgbButtons[i]) {
      device->PostButtonStateChange(
        i, static_cast<bool>(newState.rgbButtons[i] & (1 << 7)));
    }
  }

  if (mState.lZ > 0) {
    for (LONG i = 0; i < mState.lZ; i += WHEEL_DELTA) {
      device->PostVScroll(DirectInputDevice::VScrollDirection::Up);
    }
    return {};
  }

  if (mState.lZ < 0) {
    for (LONG i = 0; i > mState.lZ; i -= WHEEL_DELTA) {
      device->PostVScroll(DirectInputDevice::VScrollDirection::Down);
    }
    return {};
  }

  return {};
}

void DirectInputMouseListener::SetDataFormat() noexcept {
  this->GetDIDevice()->SetDataFormat(&c_dfDIMouse2);
}

void DirectInputMouseListener::OnAcquired() noexcept {
  const auto initResult = this->GetState(sizeof(mState), &mState);
  if (!initResult.has_value()) {
    winrt::throw_hresult(initResult.error());
  }
}

}// namespace OpenKneeboard
