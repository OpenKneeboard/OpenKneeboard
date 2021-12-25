// Copyright 2016 Patrick Mours.All rights reserved.
// Copyright(c) 2017 Advanced Micro Devices, Inc. All rights reserved.
// Copyright(c) 2021-present Fred Emmott. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// https://github.com/GPUOpen-Tools/ocat used as reference; copyright notice
// above reflects copyright notices in source material.

#include <Extras/OVR_Math.h>
#include <OVR_CAPI_D3D.H>
#include <TlHelp32.h>
#include <Unknwn.h>
#include <d3d11.h>
#include <detours.h>
#include <fmt/format.h>
#include <windows.h>
#include <winrt/base.h>

#include <filesystem>
#include <functional>
#include <vector>

#include "YAVRK/SHM.h"
#include "d3d11-offsets.h"

using SHMHeader = YAVRK::SHM::Header;
using SHMPixel = YAVRK::SHM::Pixel;
using SHMPixels = std::vector<SHMPixel>;

static_assert(sizeof(SHMPixel) == 4, "Expecting R8G8B8A8 for DirectX");
static_assert(offsetof(SHMPixel, r) == 0, "Expected red to be first byte");
static_assert(offsetof(SHMPixel, a) == 3, "Expected alpha to be last byte");

static bool g_hooked_DX = false;
static winrt::com_ptr<ID3D11Device> g_d3dDevice;
static YAVRK::SHM::Reader g_SHM;

class KneeboardRenderer {
 private:
  SHMHeader header;
  bool initialized = false;
  std::vector<winrt::com_ptr<ID3D11RenderTargetView>> RenderTargets;

 public:
  ovrTextureSwapChain SwapChain;

  KneeboardRenderer(ovrSession session, const SHMHeader& header);
  bool isCompatibleWith(const SHMHeader& header) const {
    return header.ImageWidth == this->header.ImageWidth
      && header.ImageHeight == this->header.ImageHeight;
  }
  bool Render(ovrSession session, const SHMHeader& header, const SHMPixels& pixels);
};

std::unique_ptr<KneeboardRenderer> g_Renderer;

// Not documented, but defined in libovrrt64_1.dll
// Got signature from Revive, and it matches ovr_SubmitFrame
//
// Some games (e.g. DCS World) call this directly
OVR_PUBLIC_FUNCTION(ovrResult)
ovr_SubmitFrame2(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount);

#define HOOKED_OVR_FUNCS \
  /* Our main hook: render the kneeboard */ \
  IT(ovr_EndFrame) \
  IT(ovr_SubmitFrame) \
  IT(ovr_SubmitFrame2)

static_assert(std::same_as<decltype(ovr_EndFrame), decltype(ovr_SubmitFrame)>);
static_assert(std::same_as<decltype(ovr_EndFrame), decltype(ovr_SubmitFrame2)>);

#define HOOKED_FUNCS HOOKED_OVR_FUNCS

#define UNHOOKED_OVR_FUNCS \
  /* We need the versions from the process, not whatever we had handy when \ \ \
   * \ \ \ building */ \
  IT(ovr_CreateTextureSwapChainDX) \
  IT(ovr_GetTextureSwapChainLength) \
  IT(ovr_GetTextureSwapChainBufferDX) \
  IT(ovr_GetTextureSwapChainCurrentIndex) \
  IT(ovr_CommitTextureSwapChain)

#define REAL_FUNCS HOOKED_FUNCS UNHOOKED_OVR_FUNCS

#define IT(x) decltype(&x) Real_##x = nullptr;
REAL_FUNCS
#undef IT

// methods
decltype(&IDXGISwapChain::Present) Real_IDXGISwapChain_Present = nullptr;

template <typename... T>
void dprint(fmt::format_string<T...> fmt, T&&... args) {
  auto str = fmt::format(fmt, std::forward<T>(args)...);
  str = fmt::format("[injected-kneeboard] {}\n", str);
  OutputDebugStringA(str.c_str());
}

void DetourUpdateAllThreads() {
  auto snapshot
    = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());
  THREADENTRY32 thread;
  thread.dwSize = sizeof(thread);

  DetourUpdateThread(GetCurrentThread());
  auto myProc = GetCurrentProcessId();
  auto myThread = GetCurrentThreadId();
  if (!Thread32First(snapshot, &thread)) {
    dprint("Failed to find first thread");
    return;
  }

  do {
    if (!thread.th32ThreadID) {
      continue;
    }
    if (thread.th32OwnerProcessID != myProc) {
      // CreateToolhelp32Snapshot takes a process ID, but ignores it
      continue;
    }
    if (thread.th32ThreadID == myThread) {
      continue;
    }

    auto th = OpenThread(THREAD_ALL_ACCESS, false, thread.th32ThreadID);
    DetourUpdateThread(th);
  } while (Thread32Next(snapshot, &thread));
}

class HookedMethods final {
 public:
  HRESULT __stdcall Hooked_IDXGISwapChain_Present(
    UINT SyncInterval,
    UINT Flags) {
    auto _this = reinterpret_cast<IDXGISwapChain*>(this);
    if (!g_d3dDevice) {
      _this->GetDevice(IID_PPV_ARGS(&g_d3dDevice));
      dprint(
        "Got device at {:#010x} from {}",
        (intptr_t)g_d3dDevice.get(),
        __FUNCTION__);
    }
    return std::invoke(Real_IDXGISwapChain_Present, _this, SyncInterval, Flags);
  }
};

template <class T>
bool find_function(T** funcPtrPtr, const char* lib, const char* name) {
  *funcPtrPtr = reinterpret_cast<T*>(DetourFindFunction(lib, name));

  if (*funcPtrPtr) {
    dprint(
      "- Found {} at {:#010x}", name, reinterpret_cast<intptr_t>(*funcPtrPtr));
    return true;
  }

  dprint(" - Failed to find {}", name);
  return false;
}

template <class T>
bool find_libovr_function(T** funcPtrPtr, const char* name) {
  return find_function(funcPtrPtr, "LibOVRRT64_1.dll", name);
}

static void hook_IDXGISwapChain_Present() {
  if (g_hooked_DX) {
    return;
  }
  g_hooked_DX = true;

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
  dprint("Creating temporary device and swap chain");
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
  dprint(" - got a temporary device at {:#010x}", (intptr_t)device.get());
  dprint(" - got a temporary SwapChain at {:#010x}", (intptr_t)swapchain.get());

  auto fpp = reinterpret_cast<void**>(&Real_IDXGISwapChain_Present);
  *fpp = VTable_Lookup_IDXGISwapChain_Present(swapchain.get());
  dprint(" - found IDXGISwapChain::Present at {:#010x}", (intptr_t)*fpp);
  auto mfp = &HookedMethods::Hooked_IDXGISwapChain_Present;
  dprint(
    "Hooking &{:#010x} to {:#010x}",
    (intptr_t)*fpp,
    (intptr_t) * (reinterpret_cast<void**>(&mfp)));
  DetourTransactionBegin();
  DetourUpdateAllThreads();
  auto err = DetourAttach(fpp, *(reinterpret_cast<void**>(&mfp)));
  if (err == 0) {
    dprint(" - hooked IDXGISwapChain::Present().");
  } else {
    dprint(" - failed to hook IDXGISwapChain::Present(): {}", err);
  }
  DetourTransactionCommit();
}

static void unhook_IDXGISwapChain_Present() {
  if (!g_hooked_DX) {
    return;
  }
  auto ffp = reinterpret_cast<void**>(&Real_IDXGISwapChain_Present);
  auto mfp = &HookedMethods::Hooked_IDXGISwapChain_Present;
  DetourTransactionBegin();
  DetourUpdateAllThreads();
  auto err = DetourDetach(ffp, *(reinterpret_cast<void**>(&mfp)));
  if (err) {
    dprint(" - failed to detach IDXGISwapChain");
  }
  err = DetourTransactionCommit();
  if (err) {
    dprint(" - failed to commit unhook IDXGISwapChain");
  }
  g_hooked_DX = false;
}

KneeboardRenderer::KneeboardRenderer(
  ovrSession session,
  const SHMHeader& config)
  : header(config) {
  ovrTextureSwapChainDesc kneeboardSCD = {
    .Type = ovrTexture_2D,
    .Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB,
    .ArraySize = 1,
    .Width = config.ImageWidth,
    .Height = config.ImageHeight,
    .MipLevels = 1,
    .SampleCount = 1,
    .StaticImage = false,
    .MiscFlags = ovrTextureMisc_AutoGenerateMips,
    .BindFlags = ovrTextureBind_DX_RenderTarget,
  };

  auto ret = Real_ovr_CreateTextureSwapChainDX(
    session, g_d3dDevice.get(), &kneeboardSCD, &SwapChain);
  dprint("CreateSwapChain ret: {}", ret);

  int length = -1;
  Real_ovr_GetTextureSwapChainLength(session, SwapChain, &length);
  if (length < 1) {
    dprint(" - invalid swap chain length ({})", length);
    return;
  }
  dprint(" - got swap chain length {}", length);

  RenderTargets.resize(length);
  for (int i = 0; i < length; ++i) {
    ID3D11Texture2D* texture;// todo: smart ptr?
    auto res = Real_ovr_GetTextureSwapChainBufferDX(
      session, SwapChain, i, IID_PPV_ARGS(&texture));
    if (res != 0) {
      dprint(" - failed to get swap chain buffer: {}", res);
      return;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
      .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MipSlice = 0}};

    auto hr = g_d3dDevice->CreateRenderTargetView(
      texture, &rtvd, RenderTargets.at(i).put());
    if (FAILED(hr)) {
      dprint(" - failed to create render target view");
      return;
    }
  }

  dprint("{} completed", __FUNCTION__);

  initialized = true;
}

bool KneeboardRenderer::Render(ovrSession session, const SHMHeader& config, const SHMPixels& pixels) {
  if (!initialized) {
    return false;
  }
  if (!isCompatibleWith(config)) {
    dprint("Attempted to use an incompatible renderer");
    return false;
  }
  static bool firstRun = true;
  if (firstRun) {
    dprint(
      "{} with d3ddevice at {:#010x}",
      __FUNCTION__,
      (intptr_t)g_d3dDevice.get());
  }

  int index = -1;
  Real_ovr_GetTextureSwapChainCurrentIndex(session, SwapChain, &index);
  if (index < 0) {
    dprint(" - invalid swap chain index ({})", index);
    return false;
  }

  winrt::com_ptr<ID3D11Texture2D> texture;
  Real_ovr_GetTextureSwapChainBufferDX(
    session, SwapChain, index, IID_PPV_ARGS(&texture));
  winrt::com_ptr<ID3D11DeviceContext> context;
  g_d3dDevice->GetImmediateContext(context.put());
  D3D11_BOX box {
    .left = 0,
    .top = 0,
    .front = 0,
    .right = config.ImageWidth,
    .bottom = config.ImageHeight,
    .back = 1,
  };
  context->UpdateSubresource(
    texture.get(),
    0,
    &box,
    pixels.data(),
    config.ImageWidth * sizeof(SHMPixel),
    config.ImageHeight * sizeof(SHMPixel));

  auto ret = Real_ovr_CommitTextureSwapChain(session, SwapChain);
  if (ret) {
    dprint("Commit failed with {}", ret);
  }
  if (firstRun) {
    dprint(" - success");
    firstRun = false;
  }

  return true;
}

static bool RenderKneeboard(ovrSession session, const SHMHeader& header, const SHMPixels& pixels) {
  if (!g_d3dDevice) {
    hook_IDXGISwapChain_Present();
    // Initialized by Hooked_ovrCreateTextureSwapChainDX
    return false;
  }
  unhook_IDXGISwapChain_Present();

  if (!(g_Renderer && g_Renderer->isCompatibleWith(header))) {
    g_Renderer.reset(new KneeboardRenderer(session, header));
  }

  return g_Renderer->Render(session, header, pixels);
}

static ovrResult EndFrame_Hook_Impl(
  decltype(&ovr_EndFrame) nextImpl,
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount) {
  auto shmData = g_SHM.MaybeGet();
  if (!shmData) {
    return nextImpl(
      session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
  }

  auto [config, pixels] = *shmData;

  if (!RenderKneeboard(session, config, pixels)) {
    return nextImpl(
      session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
  }

  ovrLayerQuad kneeboardLayer = {};
  kneeboardLayer.Header.Type = ovrLayerType_Quad;
  kneeboardLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
  if ((config.Flags & YAVRK::Flags::HEADLOCKED)) {
    kneeboardLayer.Header.Flags |= ovrLayerFlag_HeadLocked;
  }
  kneeboardLayer.ColorTexture = g_Renderer->SwapChain;
  kneeboardLayer.QuadPoseCenter.Position
    = {.x = config.x, .y = config.y, .z = config.z};

  OVR::Quatf orientation;
  orientation *= OVR::Quatf(OVR::Axis::Axis_X, config.rx);
  orientation *= OVR::Quatf(OVR::Axis::Axis_Y, config.ry);
  orientation *= OVR::Quatf(OVR::Axis::Axis_Z, config.rz);
  kneeboardLayer.QuadPoseCenter.Orientation = orientation;
  kneeboardLayer.QuadSize
    = {.x = config.VirtualWidth, .y = config.VirtualHeight};
  kneeboardLayer.Viewport.Pos = {.x = 0, .y = 0};
  kneeboardLayer.Viewport.Size
    = {.w = config.ImageWidth, .h = config.ImageHeight};

  std::vector<const ovrLayerHeader*> newLayers;
  if (layerCount == 0) {
    newLayers.push_back(&kneeboardLayer.Header);
  } else if (layerCount < ovrMaxLayerCount) {
    newLayers = {&layerPtrList[0], &layerPtrList[layerCount]};
    newLayers.push_back(&kneeboardLayer.Header);
  } else {
    for (unsigned int i = 0; i < layerCount; ++i) {
      if (layerPtrList[i]) {
        newLayers.push_back(layerPtrList[i]);
      }
    }

    if (newLayers.size() < ovrMaxLayerCount) {
      newLayers.push_back(&kneeboardLayer.Header);
    } else {
      dprint("Already at ovrMaxLayerCount without adding our layer");
    }
  }

  std::vector<ovrLayerEyeFov> withoutDepthInformation;
  if ((config.Flags & YAVRK::Flags::DISCARD_DEPTH_INFORMATION)) {
    for (auto i = 0; i < newLayers.size(); ++i) {
      auto layer = newLayers.at(i);
      if (layer->Type != ovrLayerType_EyeFovDepth) {
        continue;
      }

      withoutDepthInformation.push_back({});
      auto& newLayer
        = withoutDepthInformation[withoutDepthInformation.size() - 1];
      memcpy(&newLayer, layer, sizeof(ovrLayerEyeFov));
      newLayer.Header.Type = ovrLayerType_EyeFov;

      newLayers[i] = &newLayer.Header;
    }
  }

  return nextImpl(
    session,
    frameIndex,
    viewScaleDesc,
    newLayers.data(),
    static_cast<unsigned int>(newLayers.size()));
}

OVR_PUBLIC_FUNCTION(ovrResult)
Hooked_ovr_EndFrame(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount) {
  return EndFrame_Hook_Impl(
    Real_ovr_EndFrame,
    session,
    frameIndex,
    viewScaleDesc,
    layerPtrList,
    layerCount);
}

OVR_PUBLIC_FUNCTION(ovrResult)
Hooked_ovr_SubmitFrame(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount) {
  return EndFrame_Hook_Impl(
    Real_ovr_SubmitFrame,
    session,
    frameIndex,
    viewScaleDesc,
    layerPtrList,
    layerCount);
}

OVR_PUBLIC_FUNCTION(ovrResult)
Hooked_ovr_SubmitFrame2(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount) {
  return EndFrame_Hook_Impl(
    Real_ovr_SubmitFrame2,
    session,
    frameIndex,
    viewScaleDesc,
    layerPtrList,
    layerCount);
}

template <class T>
bool hook_libovr_function(const char* name, T** funcPtrPtr, T* hook) {
  if (!find_libovr_function(funcPtrPtr, name)) {
    return false;
  }
  auto err = DetourAttach((void**)funcPtrPtr, hook);
  if (!err) {
    dprint("- Hooked {} at {:#010x}", name, (intptr_t)*funcPtrPtr);
    return true;
  }

  dprint(" - Failed to hook {}: {}", name, err);
  return false;
}

#define HOOK_LIBOVR_FUNCTION(name) \
  hook_libovr_function(#name, &Real_##name, Hooked_##name)

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) {
    dprint("Attached to process.");
    DetourRestoreAfterWith();

    DetourTransactionBegin();
    DetourUpdateAllThreads();
#define IT(x) HOOK_LIBOVR_FUNCTION(x);
    HOOKED_OVR_FUNCS
#undef IT
#define IT(x) \
  Real_##x = reinterpret_cast<decltype(&x)>( \
    DetourFindFunction("LibOVRRT64_1.dll", #x));
    UNHOOKED_OVR_FUNCS
#undef IT
    auto err = DetourTransactionCommit();
    dprint("Installed hooks: {}", (int)err);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    dprint("Detaching from process...");
    DetourTransactionBegin();
    DetourUpdateAllThreads();
    g_Renderer.reset(nullptr);
#define IT(x) DetourDetach(&(PVOID&)Real_##x, Hooked_##x);
    HOOKED_FUNCS
#undef IT
    DetourTransactionCommit();
    dprint("Cleaned up Detours.");
  }
  return TRUE;
}
