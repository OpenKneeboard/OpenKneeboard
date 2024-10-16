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
#include <OpenKneeboard/D3D11.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <d3d11_3.h>

namespace OpenKneeboard::D3D11 {

DeviceContextState::DeviceContextState(ID3D11Device1* device) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::DeviceContextState(ID3D11Device1*)");

  auto featureLevel = device->GetFeatureLevel();
  check_hresult(device->CreateDeviceContextState(
    (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED)
      ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED
      : 0,
    &featureLevel,
    1,
    D3D11_SDK_VERSION,
    __uuidof(ID3D11Device),
    nullptr,
    mState.put()));
}

ScopedDeviceContextStateChange::ScopedDeviceContextStateChange(
  const winrt::com_ptr<ID3D11DeviceContext1>& context,
  DeviceContextState* newState)
  : mContext(context) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::ScopedDeviceContextStateChange()");
  if (!newState->IsValid()) {
    winrt::com_ptr<ID3D11Device> device;
    context->GetDevice(device.put());
    *newState = {device.as<ID3D11Device1>().get()};
  }
  context->SwapDeviceContextState(newState->Get(), mOriginalState.put());
}

ScopedDeviceContextStateChange::~ScopedDeviceContextStateChange() {
  OPENKNEEBOARD_TraceLoggingScope("~D3D11::ScopedDeviceContextStateChange()");
  mContext->SwapDeviceContextState(mOriginalState.get(), nullptr);
}

}// namespace OpenKneeboard::D3D11
