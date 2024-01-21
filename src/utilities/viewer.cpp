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
#include "viewer.h"

#include "viewer-d3d11.h"
#include "viewer-d3d12.h"

#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SHM/ActiveConsumers.h>
#include <OpenKneeboard/SHM/D3D11.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>

#include <shims/winrt/base.h>

#include <directxtk/ScreenGrab.h>

#include <format>
#include <memory>
#include <type_traits>

#include <D2d1.h>
#include <ShlObj_core.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>
#include <shellapi.h>

using namespace OpenKneeboard;

namespace OpenKneeboard {

Viewer::Renderer::~Renderer() = default;

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
  std::unique_ptr<Viewer::Renderer> mRenderer;
  // The renderers can't operate directly on the swapchains
  // as swapchains can't be shared.
  //
  // We *could* do D3D11 directly, but that makes it different to the otehrs, so
  // nope
  winrt::com_ptr<ID3D11Texture2D> mRendererTexture;
  winrt::handle mRendererTextureHandle;
  PixelSize mRendererTextureSize;

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

  PixelSize mSwapChainSize;
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  winrt::com_ptr<ID2D1Brush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mStreamerModeBackgroundBrush;

  HWND mHwnd {};

  UINT mDPI {USER_DEFAULT_SCREEN_DPI};

  static TestViewerWindow* gInstance;
  static LRESULT CALLBACK
  WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  void SetDPI(UINT dpi) {
    mDPI = dpi;
    mOverlayTextFormat = {};

    mDXR.mDWriteFactory->CreateTextFormat(
      L"Courier New",
      nullptr,
      DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL,
      (16.0f * mDPI) / USER_DEFAULT_SCREEN_DPI,
      L"",
      mOverlayTextFormat.put());
  }

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

    this->CreateRenderer();

    // We pass 1 as the swapchain length as we use a buffer; we need to do this
    // as swapchain textures can't be directly shared
    mRenderer->Initialize(1);

    mErrorRenderer = std::make_unique<D2DErrorRenderer>(mDXR);
    mWindowColor = GetSystemColor(COLOR_WINDOW);
    mWindowFrameColor = GetSystemColor(COLOR_WINDOWFRAME);
    mStreamerModeWindowColor = D2D1::ColorF(1.0f, 0.0f, 1.0f, 1.0f);
    mStreamerModeWindowFrameColor = mStreamerModeWindowColor;

    mDXR.mD2DDeviceContext->CreateSolidColorBrush(
      D2D1::ColorF(0.0f, 0.0f, 0.f, 0.8f), mOverlayBackground.put());
    mDXR.mD2DDeviceContext->CreateSolidColorBrush(
      D2D1::ColorF(1.0f, 1.0f, 1.f, 1.0f), mOverlayForeground.put());

    const auto dpi = GetDpiForWindow(mHwnd);
    this->SetDPI(dpi);
    if (dpi != USER_DEFAULT_SCREEN_DPI) {
      SetWindowPos(
        mHwnd,
        0,
        0,
        0,
        (768 / 2) * dpi / USER_DEFAULT_SCREEN_DPI,
        (1024 / 2) * dpi / USER_DEFAULT_SCREEN_DPI,
        SWP_NOZORDER | SWP_NOMOVE);
    }
  }

  HWND GetHWND() const {
    return mHwnd;
  }

  void CheckForUpdate() {
    auto& shm = *mRenderer->GetSHM();
    if (!shm) {
      if (mFirstDetached) {
        PaintNow();
      }
      return;
    }

    if (shm.GetRenderCacheKey(SHM::ConsumerKind::Viewer) != mRenderCacheKey) {
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

    if (
      clientSize.width > mRendererTextureSize.mWidth
      || clientSize.height > mRendererTextureSize.mHeight) {
      mRendererTexture = nullptr;
      D3D11_TEXTURE2D_DESC desc {
        .Width = clientSize.width,
        .Height = clientSize.height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = {1, 0},
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
        .MiscFlags
        = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
      };
      winrt::check_hresult(mDXR.mD3DDevice->CreateTexture2D(
        &desc, nullptr, mRendererTexture.put()));
      mRendererTextureHandle = {};
      winrt::check_hresult(
        mRendererTexture.as<IDXGIResource1>()->CreateSharedHandle(
          nullptr, GENERIC_ALL, nullptr, mRendererTextureHandle.put()));
      mRendererTextureSize = clientSize;
    }

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

      mSwapChainSize = clientSize;
      mRenderer->Initialize(desc.BufferCount);
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
    mSwapChainSize = clientSize;
    mRenderer->Initialize(swapChainDesc.BufferCount);
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

  void OnResize(const D2D1_SIZE_U& size) {
    using OpenKneeboard::SHM::ActiveConsumers;
    const auto now = ActiveConsumers::Clock::now();
    if ((now - ActiveConsumers::Get().mNonVRD3D11) > std::chrono::seconds(1)) {
      ActiveConsumers::SetNonVRPixelSize({size.width, size.height});
    }

    dprintf("Resized to {}x{}", size.width, size.height);
    mRenderer = nullptr;
    this->CreateRenderer();

    this->PaintNow();
  }

  void CaptureScreenshot() {
    const auto snapshot = mRenderer->MaybeGetSnapshot();
    if (!snapshot.IsValid()) {
      return;
    }
    if (mLayerIndex >= snapshot.GetLayerCount()) {
      return;
    }

    wchar_t* pathBuf {nullptr};
    SHGetKnownFolderPath(FOLDERID_Pictures, NULL, NULL, &pathBuf);
    if (!pathBuf) {
      return;
    }
    const std::filesystem::path baseDir {pathBuf};
    const auto path = baseDir / "OpenKneeboard"
      / std::format("capture-v{}.{}.{}.{}-{:%F-%H-%M}.dds",
                    Version::Major,
                    Version::Minor,
                    Version::Patch,
                    Version::Build,
                    std::chrono::zoned_time(
                      std::chrono::current_zone(),
                      std::chrono::system_clock::now()));
    std::filesystem::create_directories(path.parent_path());

    winrt::com_ptr<ID3D11DeviceContext> ctx;
    mDXR.mD3DDevice->GetImmediateContext(ctx.put());

    const auto texture = snapshot.GetTexture<SHM::D3D11::Texture>();

    winrt::check_hresult(DirectX::SaveDDSTextureToFile(
      ctx.get(), texture->GetD3D11Texture(), path.wstring().c_str()));
    Filesystem::OpenExplorerWithSelectedFile(path);
  }

  void OnKeyUp(uint64_t vkk) {
    switch (vkk) {
      case 'C':
        this->CaptureScreenshot();
        return;
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

  void PaintNow() noexcept {
    if (!mHwnd) {
      return;
    }
    this->InitSwapChain();

    this->PaintContent();

    if (mShowInformationOverlay) {
      this->PaintInformationOverlay();
    }

    mSwapChain->Present(0, 0);
  }

  void PaintInformationOverlay() {
    winrt::com_ptr<ID2D1Bitmap1> bitmap;
    winrt::com_ptr<IDXGISurface> surface;

    mSwapChain->GetBuffer(0, IID_PPV_ARGS(surface.put()));
    auto ctx = mDXR.mD2DDeviceContext.get();
    winrt::check_hresult(
      ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put()));
    ctx->SetTarget(bitmap.get());
    ctx->BeginDraw();
    scope_guard endDraw([&ctx]() { ctx->EndDraw(); });

    const auto clientSize = GetClientSize();
    auto text = std::format(
      L"Frame #{}, View {}",
      mRenderer->GetSHM()->GetFrameCountForMetricsOnly(),
      mLayerIndex + 1);
    const auto snapshot = mRenderer->MaybeGetSnapshot();
    if (snapshot.IsValid()) {
      const auto layer = snapshot.GetLayerConfig(mLayerIndex);
      const auto size = layer->mLocationOnTexture.mSize;
      text += std::format(L"\n{}x{}", size.mWidth, size.mHeight);
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

  void PaintContent() {
    const auto clientSize = GetClientSize();
    auto d2d = mDXR.mD2DDeviceContext.get();

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
      winrt::check_hresult(d2d->CreateBitmap(
        {20, 20},
        reinterpret_cast<BYTE*>(pixels),
        20 * sizeof(Pixel),
        D2D1::BitmapProperties(D2D1::PixelFormat(
          DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        backgroundBitmap.put()));

      mBackgroundBrush = nullptr;
      d2d->CreateBitmapBrush(
        backgroundBitmap.get(),
        D2D1::BitmapBrushProperties(
          D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP),
        reinterpret_cast<ID2D1BitmapBrush**>(mBackgroundBrush.put()));

      d2d->CreateSolidColorBrush(
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f),
        reinterpret_cast<ID2D1SolidColorBrush**>(
          mStreamerModeBackgroundBrush.put()));
    }

    winrt::com_ptr<ID2D1Bitmap1> bitmap;
    winrt::com_ptr<IDXGISurface> surface;
    mSwapChain->GetBuffer(0, IID_PPV_ARGS(surface.put()));
    winrt::check_hresult(
      d2d->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put()));
    d2d->SetTarget(bitmap.get());
    d2d->BeginDraw();
    scope_guard endDraw([d2d]() { winrt::check_hresult(d2d->EndDraw()); });

    d2d->Clear(mStreamerMode ? mStreamerModeWindowColor : mWindowColor);

    const auto snapshot = mRenderer->MaybeGetSnapshot();
    if (!snapshot.IsValid()) {
      if (!mStreamerMode) {
        mErrorRenderer->Render(
          d2d,
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
        d2d,
        std::format("No Layer {}", mLayerIndex + 1),
        {0.0f, 0.0f, float(clientSize.width), float(clientSize.height)});
      return;
    }
    winrt::check_hresult(d2d->EndDraw());
    endDraw.abandon();

    auto d3d = mDXR.mD3DDevice.get();

    const auto& layer = *snapshot.GetLayerConfig(mLayerIndex);
    mLayerID = layer.mLayerID;

    const auto& imageSize = layer.mLocationOnTexture.mSize;
    const auto scalex = float(clientSize.width) / imageSize.mWidth;
    const auto scaley = float(clientSize.height) / imageSize.mHeight;
    const auto scale = std::min(scalex, scaley);
    const auto renderWidth = static_cast<uint32_t>(imageSize.mWidth * scale);
    const auto renderHeight = static_cast<uint32_t>(imageSize.mHeight * scale);

    const auto renderLeft = (clientSize.width - renderWidth) / 2;
    const auto renderTop = (clientSize.height - renderHeight) / 2;
    const PixelRect destRect {
      {renderLeft, renderTop}, {renderWidth, renderHeight}};
    const auto sourceRect = layer.mLocationOnTexture;

    auto surfaceTex = surface.as<ID3D11Texture2D>();
    auto ctx = mDXR.mD3DImmediateContext.get();
    ctx->CopySubresourceRegion(
      mRendererTexture.get(), 0, 0, 0, 0, surfaceTex.get(), 0, nullptr);

    mRenderer->Render(
      snapshot.GetTexture<SHM::IPCClientTexture>(),
      sourceRect,
      mRendererTextureHandle.get(),
      {clientSize.width, clientSize.height},
      destRect);

    ctx->CopySubresourceRegion(
      surfaceTex.get(), 0, 0, 0, 0, mRendererTexture.get(), 0, nullptr);

    mRenderCacheKey = snapshot.GetRenderCacheKey();
  }

  void CreateRenderer() {
    enum class GraphicsAPI {
      D3D11,
      D3D12,
    };
    GraphicsAPI renderer {GraphicsAPI::D3D11};

    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    for (int i = 0; i < argc; ++i) {
      std::wstring_view arg {argv[i]};
      if (arg == L"-G") {
        std::wstring_view next {argv[++i]};
        if (next == L"D3D11") {
          renderer = GraphicsAPI::D3D11;
          continue;
        }
        if (next == L"D3D12") {
          renderer = GraphicsAPI::D3D12;
          continue;
        }
        dprintf(L"Unrecognized graphics API {}", next);
        exit(0);
      }
    }

    switch (renderer) {
      case GraphicsAPI::D3D11:
        mRenderer = std::make_unique<Viewer::D3D11Renderer>(mDXR.mD3DDevice);
        break;
      case GraphicsAPI::D3D12: {
        winrt::com_ptr<IDXGIAdapter> adapter;
        winrt::check_hresult(mDXR.mDXGIDevice->GetAdapter(adapter.put()));
        mRenderer = std::make_unique<Viewer::D3D12Renderer>(adapter.get());
        break;
      }
    }
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
    case WM_DPICHANGED: {
      gInstance->SetDPI(static_cast<UINT>(LOWORD(wParam)));
      auto rect = reinterpret_cast<RECT*>(lParam);
      SetWindowPos(
        hWnd,
        0,
        rect->left,
        rect->top,
        rect->right - rect->left,
        rect->bottom - rect->top,
        SWP_NOZORDER);
      break;
    }
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

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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
