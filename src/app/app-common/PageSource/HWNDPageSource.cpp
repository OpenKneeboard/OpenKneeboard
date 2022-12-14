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
#include <OpenKneeboard/HWNDPageSource.h>
#include <Windows.Graphics.Capture.Interop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>

namespace WGC = winrt::Windows::Graphics::Capture;
namespace WGDX = winrt::Windows::Graphics::DirectX;

namespace OpenKneeboard {

HWNDPageSource::HWNDPageSource(const DXResources& dxr, HWND window)
  : mDXR(dxr) {
  if (!window) {
    return;
  }
  auto wgiFactory = winrt::get_activation_factory<WGC::GraphicsCaptureItem>();
  auto interopFactory = wgiFactory.as<IGraphicsCaptureItemInterop>();
  WGC::GraphicsCaptureItem item {nullptr};
  interopFactory->CreateForWindow(
    window, winrt::guid_of<WGC::GraphicsCaptureItem>(), winrt::put_abi(item));
  winrt::com_ptr<IInspectable> inspectable {nullptr};
  WGDX::Direct3D11::IDirect3DDevice winrtD3D {nullptr};
  CreateDirect3D11DeviceFromDXGIDevice(
    dxr.mDXGIDevice.get(),
    reinterpret_cast<IInspectable**>(winrt::put_abi(winrtD3D)));
}

HWNDPageSource::~HWNDPageSource() = default;

PageIndex HWNDPageSource::GetPageCount() const {
  return 1;
}

D2D1_SIZE_U HWNDPageSource::GetNativeContentSize(PageIndex index) {
  return {};
}

void HWNDPageSource::RenderPage(
  ID2D1DeviceContext* ctx,
  PageIndex index,
  const D2D1_RECT_F& rect) {
}

}// namespace OpenKneeboard
