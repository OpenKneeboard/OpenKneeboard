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
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/SHM.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <format>
#include <memory>
#include <type_traits>

#include <D2d1.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>

using namespace OpenKneeboard;

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.Viewer")
 * d4df4528-1fae-5d7c-f8ac-0da5654ba6ea
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.Viewer",
  (0xd4df4528, 0x1fae, 0x5d7c, 0xf8, 0xac, 0x0d, 0xa5, 0x65, 0x4b, 0xa6, 0xea));
}// namespace OpenKneeboard

#pragma pack(push)
struct Pixel {
  uint8_t b, g, r, a;
};
#pragma pack(pop)

class TestViewerWindow final {
 private:
  bool mStreamerMode = false;
  bool mShowInformationOverlay = false;
  bool mFirstDetached = false;
  SHM::SingleBufferedReader mSHM;
  uint8_t mLayerIndex = 0;
  uint64_t mLayerID = 0;
  bool mSetInputFocus = false;
  size_t mRenderCacheKey = 0;

  D2D1_COLOR_F mWindowColor;
  D2D1_COLOR_F mStreamerModeWindowColor;
  D2D1_COLOR_F mWindowFrameColor;
  D2D1_COLOR_F mStreamerModeWindowFrameColor;

  winrt::com_ptr<ID2D1SolidColorBrush> mOverlayBackground;
  winrt::com_ptr<ID2D1SolidColorBrush> mOverlayForeground;
  winrt::com_ptr<IDWriteTextFormat> mOverlayTextFormat;

  DXResources mDXR;
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  winrt::com_ptr<ID2D1Brush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mStreamerModeBackgroundBrush;

  HWND mHwnd {};

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
      L"OpenKneeboard Viewer",
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
    mDXR.mD2DDeviceContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    mErrorRenderer = std::make_unique<D2DErrorRenderer>(mDXR);
    mWindowColor = GetSystemColor(COLOR_WINDOW);
    mWindowFrameColor = GetSystemColor(COLOR_WINDOWFRAME);
    mStreamerModeWindowColor = D2D1::ColorF(1.0f, 0.0f, 1.0f, 1.0f);
    mStreamerModeWindowFrameColor = mStreamerModeWindowColor;

    mDXR.mD2DDeviceContext->CreateSolidColorBrush(
      D2D1::ColorF(0.0f, 0.0f, 0.f, 0.8f), mOverlayBackground.put());
    mDXR.mD2DDeviceContext->CreateSolidColorBrush(
      D2D1::ColorF(1.0f, 1.0f, 1.f, 1.0f), mOverlayForeground.put());
    mDXR.mDWriteFactory->CreateTextFormat(
      L"Courier New",
      nullptr,
      DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL,
      16.0f,
      L"",
      mOverlayTextFormat.put());
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

    if (mSHM.GetRenderCacheKey(SHM::ConsumerKind::Viewer) != mRenderCacheKey) {
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

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
      .Width = clientSize.width,
      .Height = clientSize.height,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = 2,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_IGNORE,// HWND swap chain can't have alpha
    };
    mDXR.mDXGIFactory->CreateSwapChainForHwnd(
      mDXR.mD3DDevice.get(),
      mHwnd,
      &swapChainDesc,
      nullptr,
      nullptr,
      mSwapChain.put());
  }

  void OnFocus() {
    if (!(mSetInputFocus && mLayerID)) {
      return;
    }
    GameEvent {
      GameEvent::EVT_SET_INPUT_FOCUS,
      std::to_string(mLayerID),
    }
      .Send();
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

  void OnKeyUp(uint64_t vkk) {
    switch (vkk) {
      case 'S':
        mStreamerMode = !mStreamerMode;
        this->PaintNow();
        return;
      case 'I':
        mShowInformationOverlay = !mShowInformationOverlay;
        this->PaintNow();
        return;
      case 'B': {
        auto style = GetWindowLongPtrW(mHwnd, GWL_STYLE);
        if ((style & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW) {
          style &= ~WS_OVERLAPPEDWINDOW;
          style |= WS_POPUP;
        } else {
          style &= ~WS_POPUP;
          style |= WS_OVERLAPPEDWINDOW;
        }
        SetWindowLongPtrW(mHwnd, GWL_STYLE, style);
        return;
      }
    }

    if (vkk >= '1' && vkk <= '9') {
      mLayerIndex = static_cast<uint8_t>(vkk - '1');
      this->PaintNow();
      if (mSetInputFocus) {
        GameEvent {
          GameEvent::EVT_SET_INPUT_FOCUS,
          std::to_string(mLayerID),
        }
          .Send();
      }
      return;
    }
  }

  void PaintNow() {
    if (!mHwnd) {
      return;
    }
    this->InitSwapChain();

    winrt::com_ptr<IDXGISurface> surface;

    mSwapChain->GetBuffer(0, IID_PPV_ARGS(surface.put()));
    auto ctx = mDXR.mD2DDeviceContext.get();
    winrt::com_ptr<ID2D1Bitmap1> bitmap;
    winrt::check_hresult(
      ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put()));
    ctx->SetTarget(bitmap.get());

    ctx->BeginDraw();
    auto cleanup = scope_guard([&] {
      ctx->EndDraw();
      mSwapChain->Present(0, 0);
    });

    this->PaintContent(ctx);

    if (mShowInformationOverlay) {
      this->PaintInformationOverlay(ctx);
    }
  }

  void PaintInformationOverlay(ID2D1DeviceContext* ctx) {
    const auto clientSize = GetClientSize();
    auto text = std::format(
      L"Frame #{}, View {}",
      mSHM.GetFrameCountForMetricsOnly(),
      mLayerIndex + 1);
    const auto snapshot
      = mSHM.MaybeGet(mDXR.mD3DDevice.get(), SHM::ConsumerKind::Viewer);
    if (snapshot.IsValid()) {
      const auto layer = snapshot.GetLayerConfig(mLayerIndex);
      text += std::format(L"\n{}x{}", layer->mImageWidth, layer->mImageHeight);
    }

    winrt::com_ptr<IDWriteTextLayout> layout;
    mDXR.mDWriteFactory->CreateTextLayout(
      text.data(),
      text.size(),
      mOverlayTextFormat.get(),
      static_cast<FLOAT>(clientSize.width),
      static_cast<FLOAT>(clientSize.height),
      layout.put());
    layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);

    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);

    ctx->FillRectangle(
      D2D1::RectF(
        metrics.left,
        metrics.top,
        metrics.left + metrics.width,
        metrics.top + metrics.height),
      mOverlayBackground.get());
    ctx->DrawTextLayout({0.0f, 0.0f}, layout.get(), mOverlayForeground.get());
  }

  void PaintContent(ID2D1DeviceContext* ctx) {
    const auto clientSize = GetClientSize();

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

      ctx->CreateSolidColorBrush(
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f),
        reinterpret_cast<ID2D1SolidColorBrush**>(
          mStreamerModeBackgroundBrush.put()));
    }

    ctx->Clear(mStreamerMode ? mStreamerModeWindowColor : mWindowColor);

    const auto snapshot
      = mSHM.MaybeGet(mDXR.mD3DDevice.get(), SHM::ConsumerKind::Viewer);
    if (!snapshot.IsValid()) {
      if (!mStreamerMode) {
        mErrorRenderer->Render(
          ctx,
          "No Feeder",
          {0.0f, 0.0f, float(clientSize.width), float(clientSize.height)});
      }
      mFirstDetached = false;
      return;
    }
    mFirstDetached = true;

    const auto config = snapshot.GetConfig();
    mSetInputFocus = config.mVR.mEnableGazeInputFocus;

    if (mLayerIndex >= snapshot.GetLayerCount()) {
      mErrorRenderer->Render(
        ctx,
        std::format("No Layer {}", mLayerIndex + 1),
        {0.0f, 0.0f, float(clientSize.width), float(clientSize.height)});
      return;
    }
    const auto& layer = *snapshot.GetLayerConfig(mLayerIndex);
    if (!layer.IsValid()) {
      mErrorRenderer->Render(
        ctx,
        std::format("No Config For Layer {}", mLayerIndex + 1),
        {0.0f, 0.0f, float(clientSize.width), float(clientSize.height)});
      return;
    }
    mLayerID = layer.mLayerID;

    auto sharedTexture
      = snapshot.GetLayerTexture(mDXR.mD3DDevice.get(), mLayerIndex);
    if (!sharedTexture) {
      mErrorRenderer->Render(
        ctx,
        std::format("No Texture For Layer {}", mLayerIndex + 1),
        {0.0f, 0.0f, float(clientSize.width), float(clientSize.height)});
      return;
    }
    auto sharedSurface = sharedTexture.as<IDXGISurface>();

    ctx->Clear(
      mStreamerMode ? mStreamerModeWindowFrameColor : mWindowFrameColor);

    const auto scalex = float(clientSize.width) / layer.mImageWidth;
    const auto scaley = float(clientSize.height) / layer.mImageHeight;
    const auto scale = std::min(scalex, scaley);
    const auto renderWidth = static_cast<uint32_t>(layer.mImageWidth * scale);
    const auto renderHeight = static_cast<uint32_t>(layer.mImageHeight * scale);

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
      static_cast<FLOAT>(layer.mImageWidth),
      static_cast<FLOAT>(layer.mImageHeight)};
    winrt::com_ptr<ID2D1Bitmap> d2dBitmap;
    static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED);
    D2D1_BITMAP_PROPERTIES bitmapProperties {
      .pixelFormat
      = {SHM::SHARED_TEXTURE_PIXEL_FORMAT, D2D1_ALPHA_MODE_PREMULTIPLIED,},
      .dpiX = static_cast<FLOAT>(dpi),
      .dpiY = static_cast<FLOAT>(dpi),
    };
    ctx->CreateSharedBitmap(
      _uuidof(IDXGISurface),
      sharedSurface.get(),
      &bitmapProperties,
      d2dBitmap.put());

    auto bg = mStreamerMode ? mStreamerModeBackgroundBrush.get()
                            : mBackgroundBrush.get();
    // Align the top-left pixel of the brush
    bg->SetTransform(
      D2D1::Matrix3x2F::Translation({pageRect.left, pageRect.top}));

    ctx->FillRectangle(pageRect, bg);
    ctx->SetTransform(D2D1::IdentityMatrix());

    ctx->DrawBitmap(
      d2dBitmap.get(),
      &pageRect,
      1.0f,
      D2D1_INTERPOLATION_MODE_ANISOTROPIC,
      &sourceRect);
    ctx->Flush();

    mRenderCacheKey = snapshot.GetRenderCacheKey();
  }
};

TestViewerWindow* TestViewerWindow::gInstance = nullptr;

LRESULT CALLBACK TestViewerWindow::WindowProc(
  HWND hWnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam) {
  switch (uMsg) {
    case WM_SETCURSOR:
      if (LOWORD(lParam) == HTCLIENT) {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return 0;
      }
      break;
    case WM_SETFOCUS:
      gInstance->OnFocus();
      return 0;
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
    case WM_KEYUP:
      gInstance->OnKeyUp(wParam);
      return DefWindowProc(hWnd, uMsg, wParam, lParam);
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
  TraceLoggingRegister(gTraceProvider);
  const scope_guard unregisterTraceProvider(
    []() { TraceLoggingUnregister(gTraceProvider); });

  DPrintSettings::Set({.prefix = "OpenKneeboard-Viewer"});

  winrt::init_apartment(winrt::apartment_type::single_threaded);
  TestViewerWindow window(hInstance);
  ShowWindow(window.GetHWND(), nCmdShow);

  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}
