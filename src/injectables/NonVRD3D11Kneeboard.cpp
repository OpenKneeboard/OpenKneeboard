#include "NonVRD3D11Kneeboard.h"

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <d2d1.h>
#include <winrt/base.h>
#pragma comment(lib, "D2d1.lib")

namespace OpenKneeboard {

struct NonVRD3D11Kneeboard::Impl {
  SHM::Reader shm;
  winrt::com_ptr<ID2D1Factory> d2d;
};

NonVRD3D11Kneeboard::NonVRD3D11Kneeboard() : p(std::make_unique<Impl>()) {
  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, p->d2d.put());
}

NonVRD3D11Kneeboard::~NonVRD3D11Kneeboard() {
}

void NonVRD3D11Kneeboard::Unhook() {
  IDXGISwapChainPresentHook::Unhook();
}

HRESULT NonVRD3D11Kneeboard::OnPresent(
  UINT syncInterval,
  UINT flags,
  IDXGISwapChain* swapChain,
  decltype(&IDXGISwapChain::Present) next) {
  auto shm = p->shm.MaybeGet();
  if (!shm) {
    return std::invoke(next, swapChain, syncInterval, flags);
  }

  const auto& header = *shm.GetHeader();

  {
    winrt::com_ptr<ID3D11Texture2D> buffer;
    swapChain->GetBuffer(0, IID_PPV_ARGS(buffer.put()));
    if (!buffer) {
      return std::invoke(next, swapChain, syncInterval, flags);
    }

    auto surface = buffer.try_as<IDXGISurface>();
    if (!surface) {
      return std::invoke(next, swapChain, syncInterval, flags);
    }

    winrt::com_ptr<ID2D1RenderTarget> rt;
    auto ret = p->d2d->CreateDxgiSurfaceRenderTarget(
      surface.get(),
      D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_IGNORE)),
      rt.put());
    if (ret != S_OK) {
      dprintf("RT failed: {}", ret);
      return std::invoke(next, swapChain, syncInterval, flags);
    }

    static_assert(sizeof(SHM::Pixel) == 4, "Expected B8G8R8A8");
    static_assert(offsetof(SHM::Pixel, b) == 0, "Expected B8G8R8A8");

    winrt::com_ptr<ID2D1Bitmap> bitmap;
    rt->CreateBitmap(
      {header.imageWidth, header.imageHeight},
      shm.GetPixels(),
      header.imageWidth * sizeof(SHM::Pixel),
      D2D1::BitmapProperties(D2D1::PixelFormat(
        DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
      bitmap.put());

    rt->BeginDraw();
    rt->DrawBitmap(
      bitmap.get(),
      {0, 0, 500, 500},
      1.5f,
      D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    rt->EndDraw();
  }

  return std::invoke(next, swapChain, syncInterval, flags);
}

}// namespace OpenKneeboard
