#include "D3D11DeviceHook.h"

#include <OpenKneeboard/dprint.h>

#include "d3d11-offsets.h"
#include "detours-ext.h"

namespace OpenKneeboard {

namespace {
winrt::com_ptr<ID3D11Device> g_d3dDevice;

bool g_hookInstalled = false;

decltype(&IDXGISwapChain::Present) Real_IDXGISwapChain_Present = nullptr;

class HookedMethods final {
 public:
  HRESULT __stdcall Hooked_IDXGISwapChain_Present(
    UINT SyncInterval,
    UINT Flags) {
    auto _this = reinterpret_cast<IDXGISwapChain*>(this);
    if (!g_d3dDevice) {
      _this->GetDevice(IID_PPV_ARGS(&g_d3dDevice));
      dprintf(
        "Got device at {:#010x} from {}",
        (intptr_t)g_d3dDevice.get(),
        __FUNCTION__);
    }
    return std::invoke(Real_IDXGISwapChain_Present, _this, SyncInterval, Flags);
  }
};

void unhook_IDXGISwapChain_Present() {
  if (!g_hookInstalled) {
    return;
  }

  g_hookInstalled = false;

  auto ffp = reinterpret_cast<void**>(&Real_IDXGISwapChain_Present);
  auto mfp = &HookedMethods::Hooked_IDXGISwapChain_Present;
  DetourTransactionBegin();
  DetourUpdateAllThreads();
  auto err = DetourDetach(ffp, *(reinterpret_cast<void**>(&mfp)));
  if (err) {
    dprintf(" - failed to detach IDXGISwapChain");
  }
  err = DetourTransactionCommit();
  if (err) {
    dprintf(" - failed to commit unhook IDXGISwapChain");
  }
}

}// namespace

D3D11DeviceHook::D3D11DeviceHook() {
}

winrt::com_ptr<ID3D11Device> D3D11DeviceHook::maybeGet() {
  if (g_d3dDevice) {
    return g_d3dDevice;
  }
  if (g_hookInstalled) {
    return nullptr;
  }
  g_hookInstalled = true;

  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = GetForegroundWindow();
  sd.SampleDesc.Count = 1;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  sd.Windowed = TRUE;
  sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

  D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;

  winrt::com_ptr<IDXGISwapChain> swapchain;
  winrt::com_ptr<ID3D11Device> device;

  decltype(&D3D11CreateDeviceAndSwapChain) factory = nullptr;
  factory = reinterpret_cast<decltype(factory)>(
    DetourFindFunction("d3d11.dll", "D3D11CreateDeviceAndSwapChain"));
  dprintf("Creating temporary device and swap chain");
  auto ret = factory(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    D3D11_CREATE_DEVICE_DEBUG,
    &level,
    1,
    D3D11_SDK_VERSION,
    &sd,
    swapchain.put(),
    device.put(),
    nullptr,
    nullptr);
  dprintf(" - got a temporary device at {:#010x}", (intptr_t)device.get());
  dprintf(
    " - got a temporary SwapChain at {:#010x}", (intptr_t)swapchain.get());

  auto fpp = reinterpret_cast<void**>(&Real_IDXGISwapChain_Present);
  *fpp = VTable_Lookup_IDXGISwapChain_Present(swapchain.get());
  dprintf(" - found IDXGISwapChain::Present at {:#010x}", (intptr_t)*fpp);
  auto mfp = &HookedMethods::Hooked_IDXGISwapChain_Present;
  dprintf(
    "Hooking &{:#010x} to {:#010x}",
    (intptr_t)*fpp,
    (intptr_t) * (reinterpret_cast<void**>(&mfp)));
  DetourTransactionBegin();
  DetourUpdateAllThreads();
  auto err = DetourAttach(fpp, *(reinterpret_cast<void**>(&mfp)));
  if (err == 0) {
    dprintf(" - hooked IDXGISwapChain::Present().");
  } else {
    dprintf(" - failed to hook IDXGISwapChain::Present(): {}", err);
  }
  DetourTransactionCommit();

  return nullptr;
}

D3D11DeviceHook::~D3D11DeviceHook() {
  unhook_IDXGISwapChain_Present();
}

}// namespace OpenKneeboard
