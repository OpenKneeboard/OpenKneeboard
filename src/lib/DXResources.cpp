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

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/hresult.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

// clang-format off
#include <Windows.h>
#include <winternl.h>
// clang-format on

#include <d3dkmthk.h>
#include <dxgi1_6.h>

namespace OpenKneeboard {

struct D3D11Resources::Locks {
  std::recursive_mutex mMutex;
};

struct D2DResources::Locks {
  std::mutex mCurrentDrawMutex;
  struct DrawInfo {
    std::source_location mLocation;
    DWORD mThreadID;
  };
  std::optional<DrawInfo> mCurrentDraw;
};

D3D11Resources::D3D11Resources() {
  UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  auto d3dLevel = D3D_FEATURE_LEVEL_11_1;
  UINT dxgiFlags = 0;
#ifdef DEBUG
  d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
  dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  check_hresult(
    CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(mDXGIFactory.put())));

  winrt::com_ptr<IDXGIAdapter4> adapterIt;
  for (unsigned int i = 0;
       mDXGIFactory->EnumAdapterByGpuPreference(
         i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapterIt.put()))
       == S_OK;
       ++i) {
    const scope_guard releaseIt([&]() { adapterIt = {nullptr}; });
    DXGI_ADAPTER_DESC1 desc {};
    adapterIt->GetDesc1(&desc);
    dprintf(
      L"  GPU {} (LUID {:#x}): {:04x}:{:04x}: '{}' ({}mb) {}",
      i,
      std::bit_cast<uint64_t>(desc.AdapterLuid),
      desc.VendorId,
      desc.DeviceId,
      desc.Description,
      desc.DedicatedVideoMemory / (1024 * 1024),
      (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? L" (software)" : L"");
    if (i == 0) {
      mDXGIAdapter = adapterIt;
      static_assert(sizeof(uint64_t) == sizeof(desc.AdapterLuid));
      mAdapterLUID = std::bit_cast<uint64_t>(desc.AdapterLuid);
    }

    D3DKMT_OPENADAPTERFROMLUID kmtAdapter {.AdapterLuid = desc.AdapterLuid};
    winrt::check_nt(D3DKMTOpenAdapterFromLuid(&kmtAdapter));
    const scope_guard closeKMT([&kmtAdapter]() noexcept {
      D3DKMT_CLOSEADAPTER closeAdapter {kmtAdapter.hAdapter};
      winrt::check_nt(D3DKMTCloseAdapter(&closeAdapter));
    });
    D3DKMT_WDDM_2_9_CAPS caps {};
    D3DKMT_QUERYADAPTERINFO capsQuery {
      .hAdapter = kmtAdapter.hAdapter,
      .Type = KMTQAITYPE_WDDM_2_9_CAPS,
      .pPrivateDriverData = &caps,
      .PrivateDriverDataSize = sizeof(caps),
    };
    if (D3DKMTQueryAdapterInfo(&capsQuery) != 0 /* STATUS_SUCCESS */) {
      dprint("    HAGS: driver does not support WDDM 2.9 capabilities query");
      continue;
    }

    switch (caps.HwSchSupportState) {
      case DXGK_FEATURE_SUPPORT_ALWAYS_OFF:
        dprint("    HAGS: not supported");
        break;
      case DXGK_FEATURE_SUPPORT_ALWAYS_ON:
        dprint("    HAGS: always on");
        break;
      case DXGK_FEATURE_SUPPORT_EXPERIMENTAL:
        dprintf(
          "    HAGS: {} (experimental)",
          caps.HwSchEnabled ? "enabled" : "disabled");
        break;
      case DXGK_FEATURE_SUPPORT_STABLE:
        dprintf(
          "    HAGS: {} (stable)", caps.HwSchEnabled ? "enabled" : "disabled");
        break;
    }
  }
  dprint("----------");

  winrt::com_ptr<ID3D11Device> d3d;
  winrt::com_ptr<ID3D11DeviceContext> d3dImmediateContext;
  check_hresult(D3D11CreateDevice(
    mDXGIAdapter.get(),
    // UNKNOWN is required when specifying an adapter
    D3D_DRIVER_TYPE_UNKNOWN,
    nullptr,
    d3dFlags,
    &d3dLevel,
    1,
    D3D11_SDK_VERSION,
    d3d.put(),
    nullptr,
    d3dImmediateContext.put()));
  mD3D11Device = d3d.as<ID3D11Device5>();
  mD3D11ImmediateContext = d3dImmediateContext.as<ID3D11DeviceContext4>();
  mDXGIDevice = d3d.as<IDXGIDevice2>();
  d3d.as<ID3D11Multithread>()->SetMultithreadProtected(TRUE);
#ifdef DEBUG
  const auto iq = d3d.try_as<ID3D11InfoQueue>();
  if (iq) {
    iq->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
    iq->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
    iq->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
  }
#endif

  mLocks = std::make_unique<Locks>();
}

D3D11Resources::~D3D11Resources() = default;

D2DResources::D2DResources(D3D11Resources* d3d) {
  D2D1_DEBUG_LEVEL d2dDebug = D2D1_DEBUG_LEVEL_NONE;
#ifdef DEBUG
  d2dDebug = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
  D2D1_FACTORY_OPTIONS factoryOptions {.debugLevel = d2dDebug};

  check_hresult(D2D1CreateFactory(
    D2D1_FACTORY_TYPE_MULTI_THREADED,
    __uuidof(mD2DFactory),
    &factoryOptions,
    mD2DFactory.put_void()));

  check_hresult(
    mD2DFactory->CreateDevice(d3d->mDXGIDevice.get(), mD2DDevice.put()));

  winrt::com_ptr<ID2D1DeviceContext> ctx;
  check_hresult(mD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, ctx.put()));
  mD2DDeviceContext = ctx.as<ID2D1DeviceContext5>();
  ctx->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
  // Subpixel antialiasing assumes text is aligned on pixel boundaries;
  // this isn't the case for OpenKneeboard
  ctx->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

  check_hresult(DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(mDWriteFactory.put())));

  mLocks = std::make_unique<Locks>();
}

D2DResources::~D2DResources() = default;

DXResources::DXResources() : D3D11Resources(), D2DResources(this) {
  winrt::com_ptr<ID2D1DeviceContext> d2dBackBufferContext;
  check_hresult(mD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dBackBufferContext.put()));
  mD2DBackBufferDeviceContext = d2dBackBufferContext.as<ID2D1DeviceContext5>();
  d2dBackBufferContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
  d2dBackBufferContext->SetTextAntialiasMode(
    D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

  mSpriteBatch = std::make_unique<D3D11::SpriteBatch>(mD3D11Device.get());

  mWIC = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);

  check_hresult(PdfCreateRenderer(mDXGIDevice.get(), mPDFRenderer.put()));

  check_hresult(mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), mWhiteBrush.put()));
  check_hresult(mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.8f, 1.0f, 1.0f), mHighlightBrush.put()));
  check_hresult(mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), mBlackBrush.put()));
  check_hresult(mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 0.0f, 1.0f, 0.0f), mEraserBrush.put()));

  mD2DDeviceContext->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mCursorInnerBrush.put()));
  mD2DDeviceContext->CreateSolidColorBrush(
    {1.0f, 1.0f, 1.0f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mCursorOuterBrush.put()));
}

void D2DResources::PushD2DDraw(std::source_location loc) {
  {
    std::unique_lock lock(mLocks->mCurrentDrawMutex);
    if (mLocks->mCurrentDraw) {
      const auto& prev = *mLocks->mCurrentDraw;
      dprint("Starting a D2D draw while one already in progress:");
      dprintf("First: {} (thread ID {})", prev.mLocation, GetCurrentThreadId());
      dprintf("Second: {} (thread ID {})", loc, prev.mThreadID);
      OPENKNEEBOARD_BREAK;
    } else {
      mLocks->mCurrentDraw = {loc, GetCurrentThreadId()};
    }
  }
  mD2DDeviceContext->BeginDraw();
}

HRESULT D2DResources::PopD2DDraw() {
  {
    std::unique_lock lock(mLocks->mCurrentDrawMutex);
    if (!mLocks->mCurrentDraw) {
      OPENKNEEBOARD_BREAK;
    }
    mLocks->mCurrentDraw = {};
  }
  const auto result = mD2DDeviceContext->EndDraw();
  if (result != S_OK) [[unlikely]] {
    OPENKNEEBOARD_BREAK;
  }
  return result;
}

void D3D11Resources::lock() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11Resources::lock()");

  // If we've locked D2D, we don't need to separately lock D3D; keeping it
  // here anyway as:
  // - might as well check it's in multithreaded mode in debug builds
  // - keep it in the API :)

  // If we have just a D2D lock, attempting to acquire a second can lead to
  // an error inside D2D when it tries to acquire the lock in D3D but it's
  // already active

  // In the end, we use an std::recursive_mutex anyway:
  // - it's sufficient
  // - it avoids interferring with XAML, or the WinRT PDF renderer
  mLocks->mMutex.lock();
}

void D3D11Resources::unlock() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11Resources::unlock()");
  mLocks->mMutex.unlock();
}

bool D3D11Resources::try_lock() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11Resources::try_lock()");
  return mLocks->mMutex.try_lock();
}

};// namespace OpenKneeboard
