#include "IDXGISwapChainPresentHook.h"

#include <OpenKneeboard/dprint.h>
#include <winrt/base.h>

#include "d3d11-offsets.h"
#include "detours-ext.h"

namespace OpenKneeboard {

namespace {

IDXGISwapChainPresentHook* g_hook = nullptr;
bool g_hooked = false;
uint16_t g_count = 0;

decltype(&IDXGISwapChain::Present) Real_IDXGISwapChain_Present = nullptr;

}// namespace

class IDXGISwapChainPresentHook::Impl final {
 public:
  HRESULT __stdcall Hooked_IDXGISwapChain_Present(
    UINT SyncInterval,
    UINT Flags) {
    auto _this = reinterpret_cast<IDXGISwapChain*>(this);
    if (!g_hook) {
      return std::invoke(
        Real_IDXGISwapChain_Present, _this, SyncInterval, Flags);
    }

    return g_hook->OnPresent(
      SyncInterval, Flags, _this, Real_IDXGISwapChain_Present);
  }
};

void IDXGISwapChainPresentHook::Unhook() {
  if (!g_hooked) {
    return;
  }
  g_hooked = false;

  auto ffp = reinterpret_cast<void**>(&Real_IDXGISwapChain_Present);
  auto mfp = &Impl::Hooked_IDXGISwapChain_Present;
  DetourTransactionPushBegin();
  auto err = DetourDetach(ffp, *(reinterpret_cast<void**>(&mfp)));
  if (err) {
    dprintf(" - failed to detach IDXGISwapChain");
  }
  DetourTransactionPopCommit();
}

IDXGISwapChainPresentHook::IDXGISwapChainPresentHook() {
  if (g_hook) {
    throw std::logic_error("Only one IDXGISwapChainPresentHook at a time!");
  }

  g_hook = this;
  g_hooked = true;

  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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
#ifdef DEBUG
    D3D11_CREATE_DEVICE_DEBUG,
#else
    0,
#endif
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
  auto mfp = &Impl::Hooked_IDXGISwapChain_Present;
  dprintf(
    "Hooking &{:#010x} to {:#010x}",
    (intptr_t)*fpp,
    (intptr_t) * (reinterpret_cast<void**>(&mfp)));
  DetourTransactionPushBegin();
  auto err = DetourAttach(fpp, *(reinterpret_cast<void**>(&mfp)));
  if (err == 0) {
    dprintf(" - hooked IDXGISwapChain::Present().");
  } else {
    dprintf(" - failed to hook IDXGISwapChain::Present(): {}", err);
  }
  DetourTransactionPopCommit();
}

IDXGISwapChainPresentHook::~IDXGISwapChainPresentHook() {
  UnhookAndCleanup();
}

bool IDXGISwapChainPresentHook::IsHookInstalled() const {
  return g_hooked;
}

void IDXGISwapChainPresentHook::UnhookAndCleanup() {
  Unhook();
  g_hook = nullptr;
}

}// namespace OpenKneeboard
