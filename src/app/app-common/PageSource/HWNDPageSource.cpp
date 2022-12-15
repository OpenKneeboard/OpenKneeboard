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
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>

namespace WGC = winrt::Windows::Graphics::Capture;
namespace WGDX = winrt::Windows::Graphics::DirectX;

// TODO: error handling :)

namespace OpenKneeboard {

HWNDPageSource::HWNDPageSource(const DXResources& dxr, HWND window)
  : mDXR(dxr) {
  if (!window) {
    return;
  }
  if (!WGC::GraphicsCaptureSession::IsSupported()) {
    return;
  }
  auto wgiFactory = winrt::get_activation_factory<WGC::GraphicsCaptureItem>();
  auto interopFactory = wgiFactory.as<IGraphicsCaptureItemInterop>();
  WGC::GraphicsCaptureItem item {nullptr};
  winrt::check_hresult(interopFactory->CreateForWindow(
    window, winrt::guid_of<WGC::GraphicsCaptureItem>(), winrt::put_abi(item)));

  winrt::com_ptr<IInspectable> inspectable {nullptr};
  WGDX::Direct3D11::IDirect3DDevice winrtD3D {nullptr};
  winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(
    dxr.mDXGIDevice.get(),
    reinterpret_cast<IInspectable**>(winrt::put_abi(winrtD3D))));

  mFramePool = WGC::Direct3D11CaptureFramePool::CreateFreeThreaded(
    winrtD3D, WGDX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, item.Size());
  mFramePool.FrameArrived(
    [this](const auto&, const auto&) { this->OnFrame(); });

  mCaptureSession = mFramePool.CreateCaptureSession(item);
  mCaptureSession.IsCursorCaptureEnabled(false);
  mCaptureSession.IsCursorCaptureEnabled(false);
  mCaptureSession.StartCapture();
}

HWNDPageSource::~HWNDPageSource() {
  if (mCaptureSession) {
    mCaptureSession.Close();
    mFramePool.Close();
  }
}

PageIndex HWNDPageSource::GetPageCount() const {
  if (mFramePool) {
    return 1;
  }
  return 0;
}

D2D1_SIZE_U HWNDPageSource::GetNativeContentSize(PageIndex index) {
  std::unique_lock lock(mTextureMutex);
  if (!mTexture) {
    return {};
  }
  return {mContentSize.width, mContentSize.height};
}

void HWNDPageSource::RenderPage(
  ID2D1DeviceContext* ctx,
  PageIndex index,
  const D2D1_RECT_F& rect) {
  std::unique_lock lock(mTextureMutex);
  if (!mBitmap) {
    return;
  }

  ctx->DrawBitmap(
    mBitmap.get(),
    rect,
    1.0f,
    D2D1_INTERPOLATION_MODE_ANISOTROPIC,
    D2D1_RECT_F {
      0.0f,
      0.0f,
      static_cast<FLOAT>(mContentSize.width),
      static_cast<FLOAT>(mContentSize.height),
    });
}

void HWNDPageSource::OnFrame() {
  auto frame = mFramePool.TryGetNextFrame();
  if (!frame) {
    return;
  }
  auto wgdxSurface = frame.Surface();
  auto interopSurface = wgdxSurface.as<
    ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  winrt::com_ptr<IDXGISurface> nativeSurface;
  winrt::check_hresult(
    interopSurface->GetInterface(IID_PPV_ARGS(nativeSurface.put())));
  auto d3dSurface = nativeSurface.as<ID3D11Texture2D>();
  D3D11_TEXTURE2D_DESC surfaceDesc {};
  d3dSurface->GetDesc(&surfaceDesc);
  const auto contentSize = frame.ContentSize();

  winrt::com_ptr<ID3D11DeviceContext> ctx;
  mDXR.mD3DDevice->GetImmediateContext(ctx.put());

  std::unique_lock lock(mTextureMutex);
  if (mTexture) {
    D3D11_TEXTURE2D_DESC desc {};
    mTexture->GetDesc(&desc);
    if (surfaceDesc.Width != desc.Width || surfaceDesc.Height != desc.Height) {
      mTexture = nullptr;
      mBitmap = nullptr;
    }
  }
  if (!mTexture) {
    winrt::check_hresult(
      mDXR.mD3DDevice->CreateTexture2D(&surfaceDesc, nullptr, mTexture.put()));
    winrt::check_hresult(mDXR.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
      mTexture.as<IDXGISurface>().get(), nullptr, mBitmap.put()));
  }

  mContentSize = {
    static_cast<UINT>(contentSize.Width),
    static_cast<UINT>(contentSize.Height)};
  ctx->CopyResource(mTexture.get(), d3dSurface.get());
  evNeedsRepaintEvent.Emit();
}

}// namespace OpenKneeboard
