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
#include <OpenKneeboard/scope_guard.h>
#include <dxgi1_6.h>

namespace OpenKneeboard {

DXResources DXResources::Create() {
  DXResources ret;

  UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  auto d3dLevel = D3D_FEATURE_LEVEL_11_1;
  UINT dxgiFlags = 0;
#ifdef DEBUG
  d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
  dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  winrt::check_hresult(
    CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(ret.mDXGIFactory.put())));

  winrt::com_ptr<IDXGIAdapter1> adapterIt;
  winrt::com_ptr<IDXGIAdapter1> bestAdapter;
  for (unsigned int i = 0;
       ret.mDXGIFactory->EnumAdapterByGpuPreference(
         i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapterIt.put()))
       == S_OK;
       ++i) {
    const scope_guard releaseIt([&]() { adapterIt = {nullptr}; });
    if (i == 0) {
      bestAdapter = adapterIt;
    }
    DXGI_ADAPTER_DESC1 desc {};
    adapterIt->GetDesc1(&desc);
    dprintf(
      L"  GPU {} (LUID {:#x}): {:04x}:{:04x}: '{}' ({}mb){}",
      i,
      std::bit_cast<uint64_t>(desc.AdapterLuid),
      sizeof(uint64_t),
      sizeof(LUID),
      desc.VendorId,
      desc.DeviceId,
      desc.Description,
      desc.DedicatedVideoMemory / (1024 * 1024),
      (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? L" (software)" : L"");
  }
  dprint("----------");

  winrt::com_ptr<ID3D11Device> d3d;
  winrt::check_hresult(D3D11CreateDevice(
    bestAdapter.get(),
    // UNKNOWN is required when specifying an adapter
    D3D_DRIVER_TYPE_UNKNOWN,
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
  d3d.as<ID3D10Multithread>()->SetMultithreadProtected(TRUE);

  winrt::check_hresult(
    D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, ret.mD2DFactory.put()));

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
  ret.mD2DDeviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
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

namespace {
std::mutex gCurrentDrawMutex;
struct DrawInfo {
  std::source_location mLocation;
  DWORD mThreadID;
};
std::optional<DrawInfo> gCurrentDraw;
}// namespace

void DXResources::PushD2DDraw(std::source_location loc) {
  {
    std::unique_lock lock(gCurrentDrawMutex);
    if (gCurrentDraw) {
      const auto& prev = *gCurrentDraw;
      dprintf(
        "Draw already in progress from {}:{} ({}) thread {}",
        prev.mLocation.file_name(),
        prev.mLocation.line(),
        prev.mLocation.function_name(),
        prev.mThreadID);
      OPENKNEEBOARD_BREAK;
    } else {
      gCurrentDraw = {loc, GetCurrentThreadId()};
    }
  }
  mD2DDeviceContext->BeginDraw();
}

HRESULT DXResources::PopD2DDraw() {
  {
    std::unique_lock lock(gCurrentDrawMutex);
    gCurrentDraw = {};
  }
  return mD2DDeviceContext->EndDraw();
}

static std::recursive_mutex gMutex;

void DXResources::lock() {
  // If we've locked D2D, we don't need to separately lock D3D; keeping it here
  // anyway as:
  // - might as well check it's in multithreaded mode in debug builds
  // - keep it in the API :)

  // If we have just a D2D lock, attempting to acquire a second can lead to
  // an error inside D2D when it tries to acquire the lock in D3D but it's
  // already active

  // In the end, we use an std::recursive_mutex anyway:
  // - it's sufficient
  // - it avoids interferring with XAML, or the WinRT PDF renderer
  gMutex.lock();
}

void DXResources::unlock() {
  gMutex.unlock();
}

bool DXResources::try_lock() {
  return gMutex.try_lock();
}

};// namespace OpenKneeboard
