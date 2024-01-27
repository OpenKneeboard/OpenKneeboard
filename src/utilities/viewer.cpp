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
#include "viewer-vulkan.h"

#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/RenderDoc.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SHM/ActiveConsumers.h>
#include <OpenKneeboard/SHM/D3D11.h>
#include <OpenKneeboard/Shaders/D3D/Viewer.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>

#include <shims/winrt/base.h>

#include <format>
#include <memory>
#include <type_traits>

#include <D2d1.h>
#include <D2d1_3.h>
#include <ShlObj_core.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <dwrite.h>
#include <dxgi1_6.h>
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
  winrt::com_ptr<IDXGIFactory6> mDXGIFactory;
  winrt::com_ptr<ID3D11Device5> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext4> mD3D11ImmediateContext;

  winrt::com_ptr<ID3D11VertexShader> mVertexShader;
  winrt::com_ptr<ID3D11PixelShader> mPixelShader;
  winrt::com_ptr<ID3D11InputLayout> mShaderInputLayout;
  winrt::com_ptr<ID3D11Buffer> mShaderConstantBuffer;
  winrt::com_ptr<ID3D11Buffer> mVertexBuffer;

  winrt::com_ptr<ID2D1Factory> mD2DFactory;
  winrt::com_ptr<ID2D1Device> mD2DDevice;
  winrt::com_ptr<ID2D1DeviceContext> mD2DContext;
  winrt::com_ptr<IDWriteFactory> mDWriteFactory;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;

  winrt::com_ptr<ID2D1SolidColorBrush> mOverlayBackground;
  winrt::com_ptr<ID2D1SolidColorBrush> mOverlayForeground;
  winrt::com_ptr<IDWriteTextFormat> mOverlayTextFormat;
  winrt::com_ptr<ID2D1SolidColorBrush> mErrorForeground;

  static constexpr size_t MaxRectangles = 1;
  static constexpr size_t MaxTriangles = MaxRectangles * 2;
  static constexpr size_t MaxVertices = MaxTriangles * 3;

  bool mShowInformationOverlay = false;
  bool mFirstDetached = false;
  std::unique_ptr<Viewer::Renderer> mRenderer;
  // The renderers can't operate directly on the swapchains
  // as swapchains can't be shared.
  //
  // We *could* do D3D11 directly, but that makes it different to the others, so
  // nope
  winrt::com_ptr<ID3D11Texture2D> mRendererTexture;
  winrt::handle mRendererTextureHandle;
  PixelSize mRendererTextureSize;
  winrt::com_ptr<ID3D11Fence> mFence;
  winrt::handle mFenceHandle;
  uint64_t mFenceValue {};

  uint8_t mLayerIndex = 0;
  uint64_t mLayerID = 0;
  bool mSetInputFocus = false;
  size_t mRenderCacheKey = 0;

  struct ShaderDrawInfo {
    union {
      std::array<float, 2> mDimensions;
      const std::byte PADDING[16];
    };
  };

  struct ShaderFillInfo {
    D3DCOLORVALUE mD3DCOLORVALUE0;
    D3DCOLORVALUE mD3DCOLORVALUE1;
    uint32_t mColorStride = 30;

    constexpr ShaderFillInfo() = default;

    // Create a solid fill
    constexpr ShaderFillInfo(const D3DCOLORVALUE& a)
      : mD3DCOLORVALUE0(a), mD3DCOLORVALUE1(a) {
    }

    // Create a checkerboard fill
    constexpr ShaderFillInfo(
      const D3DCOLORVALUE& a,
      const D3DCOLORVALUE& b,
      uint32_t colorStride)
      : mD3DCOLORVALUE0(a), mD3DCOLORVALUE1(b), mColorStride(colorStride) {
    }
  };

  struct Vertex {
    std::array<float, 4> mPosition;
    ShaderFillInfo mFill;
  };

  ShaderFillInfo mDefaultFill;
  inline static const ShaderFillInfo mColorKeyFill {{1, 0, 1, 1}};
  inline static const ShaderFillInfo mCheckerboardFill {
    {1, 1, 1, 1},
    {0.9, 0.9, 0.9, 1},
    /* colorStride = */ 20,
  };

  enum class FillMode {
    Default,
    Checkerboard,
    ColorKey,
  };
  static constexpr auto LastFillMode = FillMode::ColorKey;
  static constexpr auto FillModeCount
    = static_cast<std::underlying_type_t<FillMode>>(LastFillMode) + 1;

  FillMode mFillMode {FillMode::Default};

  bool mStreamerMode {false};
  FillMode mStreamerModePreviousFillMode;

  PixelSize mSwapChainSize;
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  winrt::com_ptr<ID3D11Texture2D> mWindowTexture;
  winrt::com_ptr<ID3D11RenderTargetView> mWindowRenderTargetView;
  winrt::com_ptr<ID2D1Bitmap1> mWindowBitmap;

  HWND mHwnd {};

  UINT mDPI {USER_DEFAULT_SCREEN_DPI};

  static TestViewerWindow* gInstance;
  static LRESULT CALLBACK
  WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  void SetDPI(UINT dpi) {
    mDPI = dpi;

    mOverlayTextFormat = {};
    if (mDWriteFactory) {
      mDWriteFactory->CreateTextFormat(
        L"Courier New",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        (16.0f * mDPI) / USER_DEFAULT_SCREEN_DPI,
        L"",
        mOverlayTextFormat.put());
    }
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

    UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    auto d3dLevel = D3D_FEATURE_LEVEL_11_1;
    UINT dxgiFlags = 0;
#ifdef DEBUG
    d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
    dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    winrt::check_hresult(
      CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(mDXGIFactory.put())));
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(mDXGIFactory->EnumAdapterByGpuPreference(
      0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.put())));
    {
      winrt::com_ptr<ID3D11Device> device;
      winrt::com_ptr<ID3D11DeviceContext> context;
      winrt::check_hresult(D3D11CreateDevice(
        adapter.get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        d3dFlags,
        &d3dLevel,
        1,
        D3D11_SDK_VERSION,
        device.put(),
        nullptr,
        context.put()));
      mD3D11Device = device.as<ID3D11Device5>();
      mD3D11ImmediateContext = context.as<ID3D11DeviceContext4>();
    }

    this->InitializeShaders();

    this->InitializeDirect2D();

    winrt::check_hresult(mD3D11Device->CreateFence(
      mFenceValue, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(mFence.put())));
    winrt::check_hresult(mFence->CreateSharedHandle(
      nullptr, GENERIC_ALL, nullptr, mFenceHandle.put()));

    this->CreateRenderer();

    mDefaultFill = {GetSystemColor(COLOR_WINDOW)};

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

  bool HaveDirect2D() const {
    // Incompatible due to Direct2D using undocumented
    // DXGIAdapterInternal1 interface
    if (RenderDoc::IsPresent()) {
      static bool sHaveLogged {false};
      if (!sHaveLogged) {
        dprint("Disabling Direct2D because RenderDoc is present");
      }
      return false;
    }
    return true;
  }

  void InitializeDirect2D() {
    if (!HaveDirect2D()) {
      return;
    }

    winrt::check_hresult(
      D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, mD2DFactory.put()));

    D2D1_DEBUG_LEVEL d2dDebug = D2D1_DEBUG_LEVEL_NONE;
#ifdef DEBUG
    d2dDebug = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    winrt::check_hresult(D2D1CreateDevice(
      mD3D11Device.as<IDXGIDevice>().get(),
      D2D1::CreationProperties(
        D2D1_THREADING_MODE_SINGLE_THREADED,
        d2dDebug,
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE),
      mD2DDevice.put()));

    winrt::check_hresult(mD2DDevice->CreateDeviceContext(
      D2D1_DEVICE_CONTEXT_OPTIONS_NONE, mD2DContext.put()));
    mD2DContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    mD2DContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    mD2DContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);

    winrt::check_hresult(mD2DContext->CreateSolidColorBrush(
      D2D1::ColorF(0, 0, 0, 0.8), mOverlayBackground.put()));
    winrt::check_hresult(mD2DContext->CreateSolidColorBrush(
      D2D1::ColorF(1, 1, 1, 1), mOverlayForeground.put()));

    winrt::check_hresult(DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED,
      __uuidof(IDWriteFactory),
      reinterpret_cast<IUnknown**>(mDWriteFactory.put())));

    winrt::check_hresult(mD2DContext->CreateSolidColorBrush(
      D2D1::ColorF(0, 0, 0, 1), mErrorForeground.put()));
    mErrorRenderer = std::make_unique<D2DErrorRenderer>(
      mDWriteFactory.get(), mErrorForeground.get());
  }

  HWND GetHWND() const {
    return mHwnd;
  }

  void CheckForUpdate() {
    OPENKNEEBOARD_TraceLoggingScope("Viewer::CheckForUpdate");
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

  PixelSize GetClientSize() const {
    RECT clientRect;
    GetClientRect(mHwnd, &clientRect);
    return {
      static_cast<uint32_t>(clientRect.right - clientRect.left),
      static_cast<uint32_t>(clientRect.bottom - clientRect.top),
    };
  }

  void InitializeShaders() {
    constexpr auto vs = Shaders::D3D::Viewer::VS;
    constexpr auto ps = Shaders::D3D::Viewer::PS;

    winrt::check_hresult(mD3D11Device->CreateVertexShader(
      vs.data(), vs.size(), nullptr, mVertexShader.put()));
    winrt::check_hresult(mD3D11Device->CreatePixelShader(
      ps.data(), ps.size(), nullptr, mPixelShader.put()));

    D3D11_INPUT_ELEMENT_DESC inputLayout[] {
      D3D11_INPUT_ELEMENT_DESC {
        .SemanticName = "SV_Position",
        .SemanticIndex = 0,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .AlignedByteOffset = 0,
        .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
      },
      D3D11_INPUT_ELEMENT_DESC {
        .SemanticName = "COLOR",
        .SemanticIndex = 0,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
        .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
      },
      D3D11_INPUT_ELEMENT_DESC {
        .SemanticName = "COLOR",
        .SemanticIndex = 1,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
        .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
      },
      D3D11_INPUT_ELEMENT_DESC {
        .SemanticName = "COLORSTRIDE",
        .SemanticIndex = 0,
        .Format = DXGI_FORMAT_R32_UINT,
        .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
        .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
      },
    };
    winrt::check_hresult(mD3D11Device->CreateInputLayout(
      inputLayout,
      std::size(inputLayout),
      vs.data(),
      vs.size(),
      mShaderInputLayout.put()));

    D3D11_BUFFER_DESC cbufferDesc {
      .ByteWidth = sizeof(ShaderDrawInfo),
      .Usage = D3D11_USAGE_DYNAMIC,
      .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
      .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    winrt::check_hresult(mD3D11Device->CreateBuffer(
      &cbufferDesc, nullptr, mShaderConstantBuffer.put()));

    D3D11_BUFFER_DESC vertexBufferDesc {
      .ByteWidth = sizeof(Vertex) * MaxVertices,
      .Usage = D3D11_USAGE_DYNAMIC,
      .BindFlags = D3D11_BIND_VERTEX_BUFFER,
      .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    winrt::check_hresult(mD3D11Device->CreateBuffer(
      &vertexBufferDesc, nullptr, mVertexBuffer.put()));
  }

  void StartDraw() {
    auto ctx = mD3D11ImmediateContext.get();
    const auto clientSize = this->GetClientSize();

    const D3D11_VIEWPORT viewport {
      0,
      0,
      clientSize.GetWidth<float>(),
      clientSize.GetHeight<float>(),
      0,
      1,
    };
    ctx->RSSetViewports(1, &viewport);

    const auto vertexBuffer = mVertexBuffer.get();
    const UINT vertexStride = sizeof(Vertex);
    const UINT vertexOffset = 0;
    ctx->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(mShaderInputLayout.get());

    const ShaderDrawInfo drawInfo {
      .mDimensions = {
        viewport.Width,
        viewport.Height,
      },
    };

    {
      D3D11_MAPPED_SUBRESOURCE mapping {};
      winrt::check_hresult(ctx->Map(
        mShaderConstantBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping));
      memcpy(mapping.pData, &drawInfo, sizeof(drawInfo));
      ctx->Unmap(mShaderConstantBuffer.get(), 0);
    }
    const auto cbuffer = mShaderConstantBuffer.get();

    ctx->VSSetShader(mVertexShader.get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &cbuffer);
    ctx->PSSetShader(mPixelShader.get(), nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &cbuffer);

    const auto rtv = mWindowRenderTargetView.get();
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
  }

  void InitSwapChain() {
    const auto clientSize = this->GetClientSize();

    if (clientSize == mSwapChainSize) {
      return;
    }
    OPENKNEEBOARD_TraceLoggingScope("Viewer::InitSwapChain()");

    mWindowTexture = nullptr;
    mWindowRenderTargetView = nullptr;
    mWindowBitmap = nullptr;

    if (
      clientSize.mWidth > mRendererTextureSize.mWidth
      || clientSize.mHeight > mRendererTextureSize.mHeight) {
      mRendererTexture = nullptr;
      D3D11_TEXTURE2D_DESC desc {
        .Width = clientSize.mWidth,
        .Height = clientSize.mHeight,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = {1, 0},
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
        .MiscFlags
        = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
      };
      winrt::check_hresult(
        mD3D11Device->CreateTexture2D(&desc, nullptr, mRendererTexture.put()));
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

      mSwapChain->ResizeBuffers(
        desc.BufferCount,
        clientSize.mWidth,
        clientSize.mHeight,
        mode.Format,
        desc.Flags);

      mSwapChainSize = clientSize;
      mRenderer->Initialize(desc.BufferCount);
      return;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
      .Width = clientSize.mWidth,
      .Height = clientSize.mHeight,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = 2,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_IGNORE,// HWND swap chain can't have alpha
    };
    mDXGIFactory->CreateSwapChainForHwnd(
      mD3D11Device.get(),
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
    dprintf("Resizing to {}x{}", size.width, size.height);
    using OpenKneeboard::SHM::ActiveConsumers;
    const auto now = ActiveConsumers::Clock::now();
    if ((now - ActiveConsumers::Get().mNonVRD3D11) > std::chrono::seconds(1)) {
      ActiveConsumers::SetNonVRPixelSize({size.width, size.height});
    }

    dprintf("Resized to {}x{}", size.width, size.height);

    this->PaintNow();
  }

  void CaptureScreenshot() {
    const auto snapshot = mRenderer->GetSHM()->MaybeGet();
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

    mRenderer->SaveTextureToFile(
      snapshot.GetTexture<SHM::IPCClientTexture>(), path);

    Filesystem::OpenExplorerWithSelectedFile(path);
  }

  void OnKeyUp(uint64_t vkk) {
    switch (vkk) {
      // Capture
      case 'C':
        this->CaptureScreenshot();
        return;
      // Information
      case 'I':
        mShowInformationOverlay = !mShowInformationOverlay;
        this->PaintNow();
        return;
      // Borderless
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
      // Fill
      case 'F':
        mFillMode = static_cast<FillMode>(
          (static_cast<std::underlying_type_t<FillMode>>(mFillMode) + 1)
          % FillModeCount);
        this->PaintNow();
        return;
      // Streamer
      case 'S':
        mStreamerMode = !mStreamerMode;
        if (mStreamerMode) {
          mStreamerModePreviousFillMode = mFillMode;
          mFillMode = FillMode::ColorKey;
        } else if (mFillMode == FillMode::ColorKey) {
          mFillMode = mStreamerModePreviousFillMode;
        }
        this->PaintNow();
        return;
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
    OPENKNEEBOARD_TraceLoggingScope("Viewer::PaintNow()");
    this->InitSwapChain();

    if (!mWindowTexture) {
      winrt::check_hresult(
        mSwapChain->GetBuffer(0, IID_PPV_ARGS(mWindowTexture.put())));
      mWindowRenderTargetView = nullptr;
      winrt::check_hresult(mD3D11Device->CreateRenderTargetView(
        mWindowTexture.get(), nullptr, mWindowRenderTargetView.put()));
      mWindowBitmap = nullptr;
      if (HaveDirect2D()) {
        auto surface = mWindowTexture.as<IDXGISurface1>();
        winrt::check_hresult(mD2DContext->CreateBitmapFromDxgiSurface(
          surface.get(), nullptr, mWindowBitmap.put()));
      }
    }

    this->StartDraw();

    this->PaintBackground();
    this->PaintContent();

    if (mShowInformationOverlay) {
      this->PaintInformationOverlay();
    }

    mSwapChain->Present(0, 0);
  }

  void PaintBackground() {
    ShaderFillInfo fill;
    switch (mFillMode) {
      case FillMode::Default:
        fill = mDefaultFill;
        break;
      case FillMode::Checkerboard:
        fill = mCheckerboardFill;
        break;
      case FillMode::ColorKey:
        fill = mColorKeyFill;
        break;
    }
    DrawRectangle({{0, 0}, this->GetClientSize()}, fill);
  }

  void DrawRectangle(const PixelRect& rect, const ShaderFillInfo& fill) {
    const auto left = rect.Left<float>();
    const auto top = rect.Top<float>();
    const auto right = rect.Right<float>();
    const auto bottom = rect.Bottom<float>();

    const Vertex vertices[] {
      {{left, top, 0, 1}, fill},
      {{left, bottom, 0, 1}, fill},
      {{right, top, 0, 1}, fill},
      {{right, top, 0, 1}, fill},
      {{left, bottom, 0, 1}, fill},
      {{right, bottom, 0, 1}, fill},
    };

    auto ctx = mD3D11ImmediateContext.get();
    D3D11_MAPPED_SUBRESOURCE mapping {};
    winrt::check_hresult(
      ctx->Map(mVertexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping));
    memcpy(mapping.pData, vertices, sizeof(vertices));
    ctx->Unmap(mVertexBuffer.get(), 0);

    ctx->Draw(std::size(vertices), 0);
  }

  void PaintInformationOverlay() {
    if (!HaveDirect2D()) {
      return;
    }

    auto ctx = mD2DContext.get();
    ctx->SetTarget(mWindowBitmap.get());
    ctx->BeginDraw();
    scope_guard endDraw([&ctx]() { ctx->EndDraw(); });

    const auto clientSize = GetClientSize();
    auto text = std::format(
      L"Using {}\nFrame #{}",
      mRenderer->GetName(),
      mRenderer->GetSHM()->GetFrameCountForMetricsOnly());
    const auto snapshot = mRenderer->GetSHM()->MaybeGet();
    if (snapshot.IsValid()) {
      const auto layerCount = snapshot.GetLayerCount();
      if (mLayerIndex < layerCount) {
        const auto layer = snapshot.GetLayerConfig(mLayerIndex);
        const auto size = layer->mLocationOnTexture.mSize;
        text += std::format(
          L"\nView {} of {}\n{}x{}",
          mLayerIndex + 1,
          layerCount,
          size.mWidth,
          size.mHeight);
      } else {
        text += std::format(
          L"\nView {} of {}\nINVALID", mLayerIndex + 1, layerCount);
      }
    } else {
      text += L"\nNo snapshot.";
    }

    winrt::com_ptr<IDWriteTextLayout> layout;
    mDWriteFactory->CreateTextLayout(
      text.data(),
      text.size(),
      mOverlayTextFormat.get(),
      clientSize.GetWidth<FLOAT>(),
      clientSize.GetHeight<FLOAT>(),
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

    const auto snapshot = mRenderer->GetSHM()->MaybeGet();
    if (!snapshot.IsValid()) {
      if (!mStreamerMode) {
        RenderError("No Feeder");
      }
      mFirstDetached = false;
      return;
    }
    mFirstDetached = true;

    const auto config = snapshot.GetConfig();
    mSetInputFocus = config.mVR.mEnableGazeInputFocus;

    if (mLayerIndex >= snapshot.GetLayerCount()) {
      RenderError("No Layer");
      return;
    }

    const auto& layer = *snapshot.GetLayerConfig(mLayerIndex);
    mLayerID = layer.mLayerID;

    const auto& imageSize = layer.mLocationOnTexture.mSize;
    const auto scalex = clientSize.GetWidth<float>() / imageSize.mWidth;
    const auto scaley = clientSize.GetHeight<float>() / imageSize.mHeight;
    const auto scale = std::min(scalex, scaley);
    const auto renderWidth = static_cast<uint32_t>(imageSize.mWidth * scale);
    const auto renderHeight = static_cast<uint32_t>(imageSize.mHeight * scale);

    const auto renderLeft = (clientSize.mWidth - renderWidth) / 2;
    const auto renderTop = (clientSize.mHeight - renderHeight) / 2;
    const PixelRect destRect {
      {renderLeft, renderTop}, {renderWidth, renderHeight}};
    const auto sourceRect = layer.mLocationOnTexture;

    auto ctx = mD3D11ImmediateContext.get();

    const D3D11_BOX box {
      0,
      0,
      0,
      clientSize.mWidth,
      clientSize.mHeight,
      1,
    };

    // Forcing the renderer to render on top of the background to make sure it
    // preserves the existing content; clearing is fine for VR, but for non-VR
    // we need to preserve the original background.
    ctx->CopySubresourceRegion(
      mRendererTexture.get(), 0, 0, 0, 0, mWindowTexture.get(), 0, &box);

    winrt::check_hresult(ctx->Signal(mFence.get(), ++mFenceValue));

    mFenceValue = mRenderer->Render(
      snapshot.GetTexture<SHM::IPCClientTexture>(),
      sourceRect,
      mRendererTextureHandle.get(),
      mRendererTextureSize,
      destRect,
      mFenceHandle.get(),
      mFenceValue);

    winrt::check_hresult(ctx->Wait(mFence.get(), mFenceValue));

    ctx->CopySubresourceRegion(
      mWindowTexture.get(), 0, 0, 0, 0, mRendererTexture.get(), 0, &box);

    mRenderCacheKey = snapshot.GetRenderCacheKey();
  }

  void CreateRenderer() {
    enum class GraphicsAPI {
      D3D11,
      D3D12,
      Vulkan,
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
        if (next == L"Vulkan") {
          renderer = GraphicsAPI::Vulkan;
          continue;
        }
        dprintf(L"Unrecognized graphics API {}", next);
        exit(0);
      }
    }

    switch (renderer) {
      case GraphicsAPI::D3D11:
        mRenderer = std::make_unique<Viewer::D3D11Renderer>(mD3D11Device);
        break;
      case GraphicsAPI::D3D12: {
        winrt::com_ptr<IDXGIAdapter> adapter;
        mD3D11Device.as<IDXGIDevice4>()->GetAdapter(adapter.put());
        mRenderer = std::make_unique<Viewer::D3D12Renderer>(adapter.get());
        break;
      }
      case GraphicsAPI::Vulkan: {
        winrt::com_ptr<IDXGIAdapter> adapter;
        mD3D11Device.as<IDXGIDevice4>()->GetAdapter(adapter.put());
        DXGI_ADAPTER_DESC desc;
        winrt::check_hresult(adapter->GetDesc(&desc));
        static_assert(sizeof(desc.AdapterLuid) == sizeof(uint64_t));
        mRenderer = std::make_unique<Viewer::VulkanRenderer>(
          std::bit_cast<uint64_t>(desc.AdapterLuid));
        break;
      }
    }

    // We pass 1 as the swapchain length as we use a buffer; we need to do
    // this as swapchain textures can't be directly shared
    mRenderer->Initialize(1);
  }

  void RenderError(std::string_view message) {
    if (!HaveDirect2D()) {
      return;
    }

    const auto clientSize = GetClientSize();

    auto ctx = mD2DContext.get();

    ctx->SetTarget(mWindowBitmap.get());
    ctx->BeginDraw();

    mErrorRenderer->Render(
      ctx,
      message,
      {
        0.0f,
        0.0f,
        clientSize.GetWidth<float>(),
        clientSize.GetHeight<float>(),
      });

    winrt::check_hresult(ctx->EndDraw());
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
