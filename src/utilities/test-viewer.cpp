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
#include <D2d1.h>
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/scope_guard.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>
#include <shims/winrt.h>

#include <memory>
#include <type_traits>

#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D11.lib")

using namespace OpenKneeboard;

#pragma pack(push)
struct Pixel {
  uint8_t b, g, r, a;
};
#pragma pack(pop)

class TestViewerWindow final {
 private:
  bool mFirstDetached = false;
  SHM::Reader mSHM;
  uint64_t mLastSequenceNumber = 0;

  DXResources mDXR;
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  winrt::com_ptr<ID2D1Brush> mBackgroundBrush;

  HWND mHwnd;

  static TestViewerWindow* gInstance;
  static LRESULT CALLBACK
  WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

 public:
  TestViewerWindow(HINSTANCE instance) {
    gInstance = this;
    const wchar_t CLASS_NAME[] = L"OpenKneeboard Test Viewer";
    WNDCLASS wc {
      .lpfnWndProc = WindowProc,
      .hInstance = instance,
      .lpszClassName = CLASS_NAME,
    };
    RegisterClass(&wc);
    mHwnd = CreateWindowExW(
      0,
      CLASS_NAME,
      L"OpenKneeboard Test Viewer",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      768 / 2,
      1024 / 2,
      NULL,
      NULL,
      instance,
      nullptr);
    SetTimer(mHwnd, /* id = */ 1, 1000 / 60, nullptr);

    mDXR = DXResources::Create();

    mErrorRenderer = std::make_unique<D2DErrorRenderer>(mDXR.mD2DDeviceContext);
  }

  HWND GetHWND() const {
    return mHwnd;
  }

  void CheckForUpdate() {
    if (!mSHM) {
      if (mFirstDetached) {
        PaintNow();
      }
      return;
    }

    if (mSHM.GetSequenceNumber() != mLastSequenceNumber) {
      PaintNow();
    }
  }

  D2D1_SIZE_U GetClientSize() const {
    RECT clientRect;
    GetClientRect(mHwnd, &clientRect);
    return {
      static_cast<UINT>(clientRect.right - clientRect.left),
      static_cast<UINT>(clientRect.bottom - clientRect.top),
    };
  }

  void InitSwapChain() {
    const auto clientSize = this->GetClientSize();
    if (mSwapChain) {
      DXGI_SWAP_CHAIN_DESC desc;
      mSwapChain->GetDesc(&desc);
      auto& mode = desc.BufferDesc;
      if (mode.Width == clientSize.width && mode.Height == clientSize.height) {
        return;
      }
      mBackgroundBrush = nullptr;
      mDXR.mD2DDeviceContext->SetTarget(nullptr);
      mSwapChain->ResizeBuffers(
        desc.BufferCount,
        clientSize.width,
        clientSize.height,
        mode.Format,
        desc.Flags);
      return;
    }

    static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
      .Width = clientSize.width,
      .Height = clientSize.height,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = 2,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
    };
    mDXR.mDXGIFactory->CreateSwapChainForHwnd(
      mDXR.mD3DDevice.get(),
      mHwnd,
      &swapChainDesc,
      nullptr,
      nullptr,
      mSwapChain.put());
  }

  void OnPaint() {
    PAINTSTRUCT ps;
    BeginPaint(mHwnd, &ps);
    PaintNow();
    EndPaint(mHwnd, &ps);
  }

  void OnResize(const D2D1_SIZE_U&) {
    this->PaintNow();
  }

  void PaintNow() {
    this->InitSwapChain();
    const auto clientSize = GetClientSize();

    winrt::com_ptr<IDXGISurface> surface;
    mSwapChain->GetBuffer(0, IID_PPV_ARGS(surface.put()));
    auto ctx = mDXR.mD2DDeviceContext;
    winrt::com_ptr<ID2D1Bitmap1> bitmap;
    winrt::check_hresult(
      ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put()));
    ctx->SetTarget(bitmap.get());

    if (!mBackgroundBrush) {
      winrt::com_ptr<ID2D1Bitmap> backgroundBitmap;
      Pixel pixels[20 * 20];
      for (int x = 0; x < 20; x++) {
        for (int y = 0; y < 20; y++) {
          bool white = (x < 10 && y < 10) || (x >= 10 && y >= 10);
          uint8_t value = white ? 0xff : 0xcc;
          pixels[x + (20 * y)] = {value, value, value, 0xff};
        }
      }
      ctx->CreateBitmap(
        {20, 20},
        reinterpret_cast<BYTE*>(pixels),
        20 * sizeof(Pixel),
        D2D1::BitmapProperties(D2D1::PixelFormat(
          DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        backgroundBitmap.put());

      mBackgroundBrush = nullptr;
      ctx->CreateBitmapBrush(
        backgroundBitmap.get(),
        D2D1::BitmapBrushProperties(
          D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP),
        reinterpret_cast<ID2D1BitmapBrush**>(mBackgroundBrush.put()));
    }

    bool present = true;
    ctx->BeginDraw();
    auto cleanup = scope_guard([&] {
      ctx->EndDraw();
      if (present) {
        mSwapChain->Present(0, 0);
      }
    });

    ctx->Clear(GetSystemColor(COLOR_WINDOW));

    auto snapshot = mSHM.MaybeGet();
    if (!snapshot) {
      mErrorRenderer->Render(
        L"No Feeder",
        {0.0f, 0.0f, float(clientSize.width), float(clientSize.height)});
      mFirstDetached = false;
      return;
    }
    mFirstDetached = true;

    const auto config = snapshot.GetConfig();

    if (config.imageWidth == 0 || config.imageHeight == 0) {
      mErrorRenderer->Render(
        "No Image",
        {0.0f, 0.0f, float(clientSize.width), float(clientSize.height)});
      mFirstDetached = false;
      return;
    }

    auto sharedTexture = snapshot.GetSharedTexture(mDXR.mD3DDevice.get());
    if (!sharedTexture) {
      present = false;
      return;
    }
    auto sharedSurface = sharedTexture.GetSurface();

    ctx->Clear(GetSystemColor(COLOR_WINDOWFRAME));

    const auto scalex = float(clientSize.width) / config.imageWidth;
    const auto scaley = float(clientSize.height) / config.imageHeight;
    const auto scale = std::min(scalex, scaley);
    const auto renderWidth = config.imageWidth * scale;
    const auto renderHeight = config.imageHeight * scale;

    const auto renderLeft = (clientSize.width - renderWidth) / 2;
    const auto renderTop = (clientSize.height - renderHeight) / 2;
    auto dpi = GetDpiForWindow(this->GetHWND());
    D2D1_RECT_F pageRect {
      renderLeft * (dpi / 96.0f),
      renderTop * (dpi / 96.0f),
      (renderLeft + renderWidth) * (dpi / 96.0f),
      (renderTop + renderHeight) * (dpi / 96.0f)};
    D2D1_RECT_F sourceRect {
      0,
      0,
      static_cast<FLOAT>(config.imageWidth),
      static_cast<FLOAT>(config.imageHeight)};
    winrt::com_ptr<ID2D1Bitmap> d2dBitmap;
    static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
    D2D1_PIXEL_FORMAT pixelFormat {
      DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED};
    D2D1_BITMAP_PROPERTIES bitmapProperties {
      .pixelFormat = pixelFormat,
      .dpiX = static_cast<FLOAT>(dpi),
      .dpiY = static_cast<FLOAT>(dpi),
    };
    ctx->CreateSharedBitmap(
      _uuidof(IDXGISurface), sharedSurface, &bitmapProperties, d2dBitmap.put());

    auto bg = mBackgroundBrush;
    // Align the top-left pixel of the brush
    bg->SetTransform(
      D2D1::Matrix3x2F::Translation({pageRect.left, pageRect.top}));
    ctx->FillRectangle(pageRect, bg.get());
    ctx->SetTransform(D2D1::IdentityMatrix());

    ctx->DrawBitmap(
      d2dBitmap.get(),
      &pageRect,
      1.0f,
      D2D1_INTERPOLATION_MODE_ANISOTROPIC,
      &sourceRect);
    ctx->Flush();

    mLastSequenceNumber = snapshot.GetSequenceNumber();
  }
};

TestViewerWindow* TestViewerWindow::gInstance = nullptr;

LRESULT CALLBACK TestViewerWindow::WindowProc(
  HWND hWnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam) {
  switch (uMsg) {
    case WM_PAINT:
      gInstance->OnPaint();
      return 0;
    case WM_TIMER:
      gInstance->CheckForUpdate();
      return 0;
    case WM_SIZE:
      gInstance->OnResize({
        .width = LOWORD(lParam),
        .height = HIWORD(lParam),
      });
      return 0;
    case WM_CLOSE:
      PostQuitMessage(0);
      return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }
  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  PWSTR pCmdLine,
  int nCmdShow) {
  TestViewerWindow window(hInstance);
  ShowWindow(window.GetHWND(), nCmdShow);

  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}
