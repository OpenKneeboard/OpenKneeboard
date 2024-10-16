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
#pragma once

#include <OpenKneeboard/D3D.hpp>
#include <OpenKneeboard/D3D11/SpriteBatch.hpp>

#include <shims/winrt/base.h>

#include <d3d11_3.h>

namespace OpenKneeboard::D3D11 {

/** An isolated state so we don't interfere with the app or other DLLs.
 *
 * This is intended to be used in combination with
 * ScopedDeviceContextStateChange, which will initialize it with the device if
 * needed.
 */
class DeviceContextState final {
 public:
  DeviceContextState() = default;
  DeviceContextState(ID3D11Device1*);

  inline bool IsValid() const noexcept {
    return !!mState;
  }

  inline auto Get() const noexcept {
    return mState.get();
  }

 private:
  winrt::com_ptr<ID3DDeviceContextState> mState;
};

class ScopedDeviceContextStateChange final {
 public:
  /** Switch to the provided new state.
   *
   * If `newState` is invalid, this will replace it with a new
   * `DeviceContextState` initialized with the provided context's D3D11 device.
   */
  ScopedDeviceContextStateChange(
    const winrt::com_ptr<ID3D11DeviceContext1>&,
    DeviceContextState* newState);
  ~ScopedDeviceContextStateChange();

  ScopedDeviceContextStateChange() = delete;
  ScopedDeviceContextStateChange(const ScopedDeviceContextStateChange&)
    = delete;
  ScopedDeviceContextStateChange& operator=(
    const ScopedDeviceContextStateChange&)
    = delete;

 private:
  winrt::com_ptr<ID3D11DeviceContext1> mContext;
  winrt::com_ptr<ID3DDeviceContextState> mOriginalState;
};

using Opacity = ::OpenKneeboard::D3D::Opacity;

}// namespace OpenKneeboard::D3D11
