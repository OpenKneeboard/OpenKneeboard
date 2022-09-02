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
#include <OpenKneeboard/DXResources.h>

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
  winrt::check_hresult(D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    d3dFlags,
    &d3dLevel,
    1,
    D3D11_SDK_VERSION,
    d3d.put(),
    nullptr,
    nullptr));
  ret.mD3DDevice = d3d.as<ID3D11Device2>();
  ret.mDXGIDevice = d3d.as<IDXGIDevice2>();

  winrt::check_hresult(
    D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, ret.mD2DFactory.put()));
  winrt::check_hresult(
    CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(ret.mDXGIFactory.put())));

  D2D1_DEBUG_LEVEL d2dDebug = D2D1_DEBUG_LEVEL_NONE;
#ifdef DEBUG
  d2dDebug = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
  winrt::check_hresult(D2D1CreateDevice(
    ret.mDXGIDevice.get(),
    D2D1::CreationProperties(
      D2D1_THREADING_MODE_MULTI_THREADED,
      d2dDebug,
      D2D1_DEVICE_CONTEXT_OPTIONS_NONE),
    ret.mD2DDevice.put()));

  winrt::check_hresult(ret.mD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, ret.mD2DDeviceContext.put()));
  // Subpixel antialiasing assumes text is aligned on pixel boundaries;
  // this isn't the case for OpenKneeboard
  ret.mD2DDeviceContext->SetTextAntialiasMode(
    D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

  winrt::check_hresult(DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(ret.mDWriteFactory.put())));

  ret.mWIC
    = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);

  return ret;
}

};// namespace OpenKneeboard
