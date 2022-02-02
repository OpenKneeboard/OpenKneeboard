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
#include "DXResources.h"

namespace OpenKneeboard {

DXResources DXResources::Create() {
  DXResources ret;

  UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  UINT dxgiFlags = 0;
#ifdef DEBUG
  d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
  dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  auto d3dLevel = D3D_FEATURE_LEVEL_11_0;

  winrt::com_ptr<ID3D11Device> d3d;
  D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    d3dFlags,
    &d3dLevel,
    1,
    D3D11_SDK_VERSION,
    d3d.put(),
    nullptr,
    nullptr);
  ret.mD3DDevice = d3d.as<ID3D11Device2>();
  ret.mDXGIDevice = d3d.as<IDXGIDevice2>();

  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, ret.mD2DFactory.put());
  CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(ret.mDXGIFactory.put()));

  return ret;
}

};// namespace OpenKneeboard
