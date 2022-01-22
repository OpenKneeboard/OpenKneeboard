#include "IDXGISwapChainPresentHook.h"

#include <OpenKneeboard/dprint.h>
#include <d3d11.h>
#include <psapi.h>
#include <winrt/base.h>

#include <bit>
#include <utility>

#include "detours-ext.h"
#include "dxgi-offsets.h"
#include "function-patterns.h"

namespace OpenKneeboard {

namespace {

void* Find_SteamOverlay_IDXGISwapChain_Present() {
  // We're trying to find a non-exported function, so we need to try and figure
  // out where it is based on what it looks like.
  // clang-format off
  const unsigned char pattern[] = {
    // Looking for the function prologue: save callee-preserved
    // registers that the function uses; these are likely used
    // by Steam's trampoline calling convention
    0x48, 0x89, 0x6c, 0x24, '?', // MOV qword (stack offset) RBP
    0x48, 0x89, 0x74, 0x24, '?', // MOV qword (stack offset) RSI
    0x41, 0x56, // PUSH R14
    // ... then adjust the stack by the fixed allocation size
    0x48, 0x83, 0xec, '?', // SUB RSP, (fixed allocation size)
    // End prologue: start doing stuff
    0x41, 0x8b, 0xe8, // MOV EBP, R8D (arg3: UINT Flags)
    0x8b, 0xf2, // MOV ESI, EDX (arg2: UINT SyncInterval)
    0x4c, 0x8b, 0xf1, // MOV R14 (arg1: IDXGISwapChain* this)
    0x41, 0xf6, 0xc0, 0x01 // TEST EBP,0x1 // TEST flags & DXGI_PRESENT_TEST
  };
  // clang-format on
  bool foundMultiple = false;
  dprint("Looking for SteamVR overlay");
  auto func = FindFunctionPatternInModule(
    "GameOverlayRenderer64", {pattern, sizeof(pattern)}, &foundMultiple);
  if (foundMultiple) {
    dprintf("Found multiple potential Steam overlay functions :'(");
    return nullptr;
  }
  return func;
}

}// namespace

struct IDXGISwapChainPresentHook::Impl {
 public:
  Impl(const Callbacks&);
  ~Impl();

  void UninstallHook();

 private:
  static Impl* gInstance;
  static decltype(&IDXGISwapChain::Present) Next_IDXGISwapChain_Present;

  void InstallSteamOverlayHook(void* steamHookAddress);
  void InstallVTableHook();

  Callbacks mCallbacks;

  HRESULT __stdcall Hooked_IDXGISwapChain_Present(
    UINT SyncInterval,
    UINT Flags);
};
IDXGISwapChainPresentHook::Impl* IDXGISwapChainPresentHook::Impl::gInstance
  = nullptr;
decltype(&IDXGISwapChain::Present)
  IDXGISwapChainPresentHook::Impl::Next_IDXGISwapChain_Present
  = nullptr;

IDXGISwapChainPresentHook::IDXGISwapChainPresentHook() {
  dprint(__FUNCTION__);
}

void IDXGISwapChainPresentHook::InstallHook(const Callbacks& cb){
  p = std::make_unique<Impl>(cb);
}

IDXGISwapChainPresentHook::~IDXGISwapChainPresentHook() {
  dprintf("{} {:#018x}", __FUNCTION__, (int64_t)this);
  this->UninstallHook();
}

void IDXGISwapChainPresentHook::UninstallHook() {
  if (p) {
    p->UninstallHook();
  }
}

void IDXGISwapChainPresentHook::Impl::UninstallHook() {
  if (gInstance != this) {
    return;
  }

  DetourSingleDetach(
    reinterpret_cast<void**>(&Next_IDXGISwapChain_Present),
    std::bit_cast<void*>(&Impl::Hooked_IDXGISwapChain_Present));
  gInstance = nullptr;

  dprint("Detached IDXGISwapChain::Present hook");
}

IDXGISwapChainPresentHook::Impl::Impl(const Callbacks& callbacks)
  : mCallbacks(callbacks) {
  if (gInstance) {
    throw std::logic_error("Only one IDXGISwapChainPresentHook at a time");
  }
  gInstance = this;

  auto addr = Find_SteamOverlay_IDXGISwapChain_Present();
  if (addr) {
    dprintf(
      "Installing IDXGISwapChain::Present hook via Steam overlay at "
      "{:#018x}...",
      (int64_t)addr);
    this->InstallSteamOverlayHook(addr);
    return;
  }
  dprint("Installing IDXGISwapChain::Present hook via VTable...");
  this->InstallVTableHook();
}

IDXGISwapChainPresentHook::Impl::~Impl() {
  this->UninstallHook();
}

void IDXGISwapChainPresentHook::Impl::InstallVTableHook() {
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

  winrt::com_ptr<ID3D11Device> device;
  winrt::com_ptr<IDXGISwapChain> swapchain;

  UINT flags = 0;
#ifdef DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  decltype(&D3D11CreateDeviceAndSwapChain) factory = nullptr;
  factory = reinterpret_cast<decltype(factory)>(
    DetourFindFunction("d3d11.dll", "D3D11CreateDeviceAndSwapChain"));
  dprintf("Creating temporary device and swap chain");
  auto ret = factory(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    flags,
    &level,
    1,
    D3D11_SDK_VERSION,
    &sd,
    swapchain.put(),
    device.put(),
    nullptr,
    nullptr);
  dprintf(" - got a temporary device at {:#018x}", (intptr_t)device.get());
  dprintf(
    " - got a temporary SwapChain at {:#018x}", (intptr_t)swapchain.get());

  auto fpp = reinterpret_cast<void**>(&Next_IDXGISwapChain_Present);
  *fpp = VTable_Lookup_IDXGISwapChain_Present(swapchain.get());
  dprintf(" - found IDXGISwapChain::Present at {:#018x}", (intptr_t)*fpp);
  auto err = DetourSingleAttach(
    fpp, std::bit_cast<void*>(&Impl::Hooked_IDXGISwapChain_Present));
  if (err == 0) {
    dprintf(" - hooked IDXGISwapChain::Present().");
  } else {
    dprintf(" - failed to hook IDXGISwapChain::Present(): {}", err);
  }
}

void IDXGISwapChainPresentHook::Impl::InstallSteamOverlayHook(
  void* steamHookAddress) {
  auto fpp = reinterpret_cast<void**>(&Next_IDXGISwapChain_Present);
  *fpp = steamHookAddress;
  dprintf("Hooking Steam overlay at {:#018x}", (intptr_t)*fpp);
  auto err = DetourSingleAttach(
    fpp, std::bit_cast<void*>(&Impl::Hooked_IDXGISwapChain_Present));
  if (err == 0) {
    dprint(" - hooked Steam Overlay IDXGISwapChain::Present hook.");
  } else {
    dprintf(" - failed to hook Steam Overlay: {}", err);
  }
}

HRESULT __stdcall IDXGISwapChainPresentHook::Impl::
  Hooked_IDXGISwapChain_Present(UINT SyncInterval, UINT Flags) {
  auto this_ = reinterpret_cast<IDXGISwapChain*>(this);
  if (!(gInstance && gInstance->mCallbacks.onPresent)) [[unlikely]] {
    return std::invoke(Next_IDXGISwapChain_Present, this_, SyncInterval, Flags);
  }

  return gInstance->mCallbacks.onPresent(
    this_, SyncInterval, Flags, Next_IDXGISwapChain_Present);
}

}// namespace OpenKneeboard
