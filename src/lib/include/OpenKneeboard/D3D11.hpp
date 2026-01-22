// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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

  inline bool IsValid() const noexcept { return !!mState; }

  inline auto Get() const noexcept { return mState.get(); }

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
  ScopedDeviceContextStateChange(const ScopedDeviceContextStateChange&) =
    delete;
  ScopedDeviceContextStateChange& operator=(
    const ScopedDeviceContextStateChange&) = delete;

 private:
  winrt::com_ptr<ID3D11DeviceContext1> mContext;
  winrt::com_ptr<ID3DDeviceContextState> mOriginalState;
};

using Opacity = ::OpenKneeboard::D3D::Opacity;

}// namespace OpenKneeboard::D3D11
