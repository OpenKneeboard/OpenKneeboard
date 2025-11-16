// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include "IDXGISwapChainHook.hpp"

#include "FindMainWindow.hpp"
#include "detours-ext.hpp"
#include "dxgi-offsets.h"
#include "function-patterns.hpp"

#include <OpenKneeboard/dprint.hpp>

#include <shims/winrt/base.h>

#include <d3d11.h>
#include <psapi.h>

#include <bit>
#include <utility>

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
    dprint("Found multiple potential Steam overlay functions :'(");
    return nullptr;
  }
  return func;
}

}// namespace

struct IDXGISwapChainHook::Impl {
 public:
  Impl(const Callbacks&);
  ~Impl();

  void UninstallHook();

  Impl() = delete;
  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl& operator=(Impl&&) = delete;

 private:
  static Impl* gInstance;
  static decltype(&IDXGISwapChain::Present) Next_IDXGISwapChain_Present;
  static decltype(&IDXGISwapChain::ResizeBuffers)
    Next_IDXGISwapChain_ResizeBuffers;

  void InstallSteamOverlayHook(void* steamHookAddress);
  void InstallVTableHook();
  bool InstallVTableHookOnce();

  Callbacks mCallbacks;

  HRESULT __stdcall Hooked_IDXGISwapChain_Present(
    UINT SyncInterval,
    UINT Flags);

  HRESULT __stdcall Hooked_IDXGISwapChain_ResizeBuffers(
    UINT BufferCount,
    UINT Width,
    UINT Height,
    DXGI_FORMAT NewFormat,
    UINT SwapChainFlags);
};
IDXGISwapChainHook::Impl* IDXGISwapChainHook::Impl::gInstance = nullptr;
decltype(&IDXGISwapChain::Present)
  IDXGISwapChainHook::Impl::Next_IDXGISwapChain_Present
  = nullptr;
decltype(&IDXGISwapChain::ResizeBuffers)
  IDXGISwapChainHook::Impl::Next_IDXGISwapChain_ResizeBuffers
  = nullptr;

IDXGISwapChainHook::IDXGISwapChainHook() {
  dprint(__FUNCTION__);
}

void IDXGISwapChainHook::InstallHook(const Callbacks& cb) {
  p = std::make_unique<Impl>(cb);
}

IDXGISwapChainHook::~IDXGISwapChainHook() {
  dprint("{} {:#018x}", __FUNCTION__, (int64_t)this);
  this->UninstallHook();
}

void IDXGISwapChainHook::UninstallHook() {
  if (p) {
    p->UninstallHook();
  }
}

void IDXGISwapChainHook::Impl::UninstallHook() {
  if (gInstance != this) {
    return;
  }

  DetourSingleDetach(
    reinterpret_cast<void**>(&Next_IDXGISwapChain_Present),
    std::bit_cast<void*>(&Impl::Hooked_IDXGISwapChain_Present));
  DetourSingleDetach(
    reinterpret_cast<void**>(&Next_IDXGISwapChain_ResizeBuffers),
    std::bit_cast<void*>(&Impl::Hooked_IDXGISwapChain_ResizeBuffers));
  gInstance = nullptr;

  dprint("Detached IDXGISwapChain hooks");
}

IDXGISwapChainHook::Impl::Impl(const Callbacks& callbacks)
  : mCallbacks(callbacks) {
  if (gInstance) {
    throw std::logic_error("Only one IDXGISwapChainHook at a time");
  }
  gInstance = this;

  auto addr = Find_SteamOverlay_IDXGISwapChain_Present();
  if (addr) {
    dprint(
      "Installing IDXGISwapChain::Present hook via Steam overlay at "
      "{:#018x}...",
      (int64_t)addr);
    this->InstallSteamOverlayHook(addr);
    return;
  }
  dprint("Installing IDXGISwapChain hooks via VTable...");
  this->InstallVTableHook();
}

IDXGISwapChainHook::Impl::~Impl() {
  this->UninstallHook();
}

void IDXGISwapChainHook::Impl::InstallVTableHook() {
  if (InstallVTableHookOnce()) {
    return;
  }
  for (int i = 0; i < 30; i++) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    dprint("Trying again...");
    if (InstallVTableHookOnce()) {
      return;
    }
  }
  dprint("... giving up on VTable hook.");
}

bool IDXGISwapChainHook::Impl::InstallVTableHookOnce() {
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = FindMainWindow();
  sd.SampleDesc.Count = 1;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  sd.Windowed = TRUE;
  sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

  if (!sd.OutputWindow) {
    return false;
  }

  D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_1;

  winrt::com_ptr<ID3D11Device> device;
  winrt::com_ptr<IDXGISwapChain> swapchain;

  UINT flags = 0;
#ifdef DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  decltype(&D3D11CreateDeviceAndSwapChain) factory = nullptr;
  factory = reinterpret_cast<decltype(factory)>(
    DetourFindFunction("d3d11.dll", "D3D11CreateDeviceAndSwapChain"));
  dprint("Creating temporary device and swap chain");
  auto error = factory(
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
  if (!(device && swapchain)) {
    dprint(" - failed to get D3D11 device and swapchain: {}", error);
    return false;
  }

  dprint(" - got a temporary device at {:#018x}", (intptr_t)device.get());
  dprint(" - got a temporary SwapChain at {:#018x}", (intptr_t)swapchain.get());

  auto presentFPP = reinterpret_cast<void**>(&Next_IDXGISwapChain_Present);
  *presentFPP = VTable_Lookup_IDXGISwapChain_Present(swapchain.get());
  dprint(" - found IDXGISwapChain::Present at {:#018x}", (intptr_t)*presentFPP);
  auto resizeBuffersFPP
    = reinterpret_cast<void**>(&Next_IDXGISwapChain_ResizeBuffers);
  *resizeBuffersFPP
    = VTable_Lookup_IDXGISwapChain_ResizeBuffers(swapchain.get());
  dprint(
    " - found IDXGISwapChain::ResizeBuffers at {:#018x}",
    (intptr_t)*resizeBuffersFPP);

  {
    DetourTransaction dt;
    auto err = DetourAttach(
      presentFPP, std::bit_cast<void*>(&Impl::Hooked_IDXGISwapChain_Present));
    if (err == 0) {
      dprint(" - hooked IDXGISwapChain::Present().");
    } else {
      dprint(" - failed to hook IDXGISwapChain::Present(): {}", err);
      return true;
    }

    err = DetourAttach(
      resizeBuffersFPP,
      std::bit_cast<void*>(&Impl::Hooked_IDXGISwapChain_ResizeBuffers));
    if (err == 0) {
      dprint(" - hooked IDXGISwapChain::ResizeBuffers().");
    } else {
      dprint(" - failed to hook IDXGISwapChain::ResizeBuffers(): {}", err);
      return true;
    }
  }

  return true;
}

void IDXGISwapChainHook::Impl::InstallSteamOverlayHook(void* steamHookAddress) {
  auto fpp = reinterpret_cast<void**>(&Next_IDXGISwapChain_Present);
  *fpp = steamHookAddress;
  dprint("Hooking Steam overlay at {:#018x}", (intptr_t)*fpp);
  auto err = DetourSingleAttach(
    fpp, std::bit_cast<void*>(&Impl::Hooked_IDXGISwapChain_Present));
  if (err == 0) {
    dprint(" - hooked Steam Overlay IDXGISwapChain::Present hook.");
  } else {
    dprint(" - failed to hook Steam Overlay: {}", err);
  }
}

HRESULT __stdcall IDXGISwapChainHook::Impl::Hooked_IDXGISwapChain_Present(
  UINT SyncInterval,
  UINT Flags) {
  auto this_ = reinterpret_cast<IDXGISwapChain*>(this);
  if (!(gInstance && gInstance->mCallbacks.onPresent)) [[unlikely]] {
    return std::invoke(Next_IDXGISwapChain_Present, this_, SyncInterval, Flags);
  }

  return gInstance->mCallbacks.onPresent(
    this_, SyncInterval, Flags, Next_IDXGISwapChain_Present);
}

HRESULT __stdcall IDXGISwapChainHook::Impl::Hooked_IDXGISwapChain_ResizeBuffers(
  UINT bufferCount,
  UINT width,
  UINT height,
  DXGI_FORMAT newFormat,
  UINT swapChainFlags) {
  auto this_ = reinterpret_cast<IDXGISwapChain*>(this);
  if (!(gInstance && gInstance->mCallbacks.onResizeBuffers)) [[unlikely]] {
    return std::invoke(
      Next_IDXGISwapChain_ResizeBuffers,
      this_,
      bufferCount,
      width,
      height,
      newFormat,
      swapChainFlags);
  }

  return gInstance->mCallbacks.onResizeBuffers(
    this_,
    bufferCount,
    width,
    height,
    newFormat,
    swapChainFlags,
    Next_IDXGISwapChain_ResizeBuffers);
}

}// namespace OpenKneeboard
