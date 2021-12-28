
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

#include <OVR_CAPI_D3D.H>
#include <Unknwn.h>
#include <d3d11.h>
#include <fmt/format.h>
#include <windows.h>
#include <winrt/base.h>

#include "detours-ext.h"

#pragma warning(push)
// lossy conversions (double -> T)
#pragma warning(disable : 4244)
#include <Extras/OVR_Math.h>
#pragma warning(pop)

#include <filesystem>
#include <functional>
#include <vector>

#include "D3D11DeviceHook.h"
#include "OpenKneeboard/SHM.h"
#include "OpenKneeboard/dprint.h"

using SHMHeader = OpenKneeboard::SHM::Header;
using SHMPixel = OpenKneeboard::SHM::Pixel;

using namespace OpenKneeboard;

static_assert(sizeof(SHMPixel) == 4, "Expecting R8G8B8A8 for DirectX");
static_assert(offsetof(SHMPixel, r) == 0, "Expected red to be first byte");
static_assert(offsetof(SHMPixel, a) == 3, "Expected alpha to be last byte");

static SHM::Reader g_SHM;
static D3D11DeviceHook g_d3dDevice;

class KneeboardRenderer {
 private:
  SHMHeader mHeader;
  bool mInitialized = false;
  winrt::com_ptr<ID3D11Device> mD3dDevice;
  std::vector<winrt::com_ptr<ID3D11RenderTargetView>> mRenderTargets;

 public:
  ovrTextureSwapChain SwapChain;

  KneeboardRenderer(
    ovrSession session,
    const winrt::com_ptr<ID3D11Device>&,
    const SHMHeader& header);
  bool isCompatibleWith(const SHMHeader& header) const {
    return header.ImageWidth == mHeader.ImageWidth
      && header.ImageHeight == mHeader.ImageHeight;
  }
  bool Render(ovrSession session, const SHM::Snapshot& snapshot);
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

template <class T>
bool find_function(T** funcPtrPtr, const char* lib, const char* name) {
  *funcPtrPtr = reinterpret_cast<T*>(DetourFindFunction(lib, name));

  if (*funcPtrPtr) {
    dprintf(
      "- Found {} at {:#010x}", name, reinterpret_cast<intptr_t>(*funcPtrPtr));
    return true;
  }

  dprintf(" - Failed to find {}", name);
  return false;
}

template <class T>
bool find_libovr_function(T** funcPtrPtr, const char* name) {
  return find_function(funcPtrPtr, "LibOVRRT64_1.dll", name);
}

KneeboardRenderer::KneeboardRenderer(
  ovrSession session,
  const winrt::com_ptr<ID3D11Device>& d3dDevice,
  const SHMHeader& config)
  : mHeader(config), mD3dDevice(d3dDevice) {
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
    session, d3dDevice.get(), &kneeboardSCD, &SwapChain);
  dprintf("CreateSwapChain ret: {}", ret);

  int length = -1;
  Real_ovr_GetTextureSwapChainLength(session, SwapChain, &length);
  if (length < 1) {
    dprintf(" - invalid swap chain length ({})", length);
    return;
  }
  dprintf(" - got swap chain length {}", length);

  mRenderTargets.resize(length);
  for (int i = 0; i < length; ++i) {
    ID3D11Texture2D* texture;// todo: smart ptr?
    auto res = Real_ovr_GetTextureSwapChainBufferDX(
      session, SwapChain, i, IID_PPV_ARGS(&texture));
    if (res != 0) {
      dprintf(" - failed to get swap chain buffer: {}", res);
      return;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
      .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MipSlice = 0}};

    auto hr = d3dDevice->CreateRenderTargetView(
      texture, &rtvd, mRenderTargets.at(i).put());
    if (FAILED(hr)) {
      dprintf(" - failed to create render target view");
      return;
    }
  }

  dprintf("{} completed", __FUNCTION__);

  mInitialized = true;
}

bool KneeboardRenderer::Render(
  ovrSession session,
  const SHM::Snapshot& snapshot) {
  if (!mInitialized) {
    return false;
  }

  auto& config = *snapshot.GetHeader();

  if (!isCompatibleWith(config)) {
    dprintf("Attempted to use an incompatible renderer");
    return false;
  }
  static bool firstRun = true;
  if (firstRun) {
    dprintf(
      "{} with d3ddevice at {:#010x}",
      __FUNCTION__,
      (intptr_t)mD3dDevice.get());
  }

  int index = -1;
  Real_ovr_GetTextureSwapChainCurrentIndex(session, SwapChain, &index);
  if (index < 0) {
    dprintf(" - invalid swap chain index ({})", index);
    return false;
  }

  winrt::com_ptr<ID3D11Texture2D> texture;
  Real_ovr_GetTextureSwapChainBufferDX(
    session, SwapChain, index, IID_PPV_ARGS(&texture));
  winrt::com_ptr<ID3D11DeviceContext> context;
  mD3dDevice->GetImmediateContext(context.put());
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
    snapshot.GetPixels(),
    config.ImageWidth * sizeof(SHMPixel),
    0);

  auto ret = Real_ovr_CommitTextureSwapChain(session, SwapChain);
  if (ret) {
    dprintf("Commit failed with {}", ret);
  }
  if (firstRun) {
    dprintf(" - success");
    firstRun = false;
  }

  return true;
}

static bool RenderKneeboard(
  ovrSession session,
  const SHM::Snapshot& snapshot) {
  auto d3d = g_d3dDevice.getOrHook();
  if (!d3d) {
    return false;
  }

  if (!snapshot) {
    return false;
  }

  if (!(g_Renderer && g_Renderer->isCompatibleWith(*snapshot.GetHeader()))) {
    dprint("Incompatible header change, resetting");
    g_Renderer.reset(new KneeboardRenderer(session, d3d, *snapshot.GetHeader()));
  }

  return g_Renderer->Render(session, snapshot);
}

static ovrResult EndFrame_Hook_Impl(
  decltype(&ovr_EndFrame) nextImpl,
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount) {
  auto snapshot = g_SHM.MaybeGet();
  if (!snapshot) {
    return nextImpl(
      session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
  }

  if (!RenderKneeboard(session, snapshot)) {
    return nextImpl(
      session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
  }

  const auto& config = *snapshot.GetHeader();

  ovrLayerQuad kneeboardLayer = {};
  kneeboardLayer.Header.Type = ovrLayerType_Quad;
  kneeboardLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
  if ((config.Flags & OpenKneeboard::Flags::HEADLOCKED)) {
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
      dprintf("Already at ovrMaxLayerCount without adding our layer");
    }
  }

  std::vector<ovrLayerEyeFov> withoutDepthInformation;
  if ((config.Flags & OpenKneeboard::Flags::DISCARD_DEPTH_INFORMATION)) {
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
    dprintf("- Hooked {} at {:#010x}", name, (intptr_t)*funcPtrPtr);
    return true;
  }

  dprintf(" - Failed to hook {}: {}", name, err);
  return false;
}

#define HOOK_LIBOVR_FUNCTION(name) \
  hook_libovr_function(#name, &Real_##name, Hooked_##name)

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) {
    OpenKneeboard::DPrintSettings::Set({
      .Prefix = "OpenKneeboard-Oculus-D3D11",
    });
    dprintf("Attached to process.");
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
    dprintf("Installed hooks: {}", (int)err);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    dprintf("Detaching from process...");
    DetourTransactionBegin();
    DetourUpdateAllThreads();
    g_Renderer.reset(nullptr);
#define IT(x) DetourDetach(&(PVOID&)Real_##x, Hooked_##x);
    HOOKED_FUNCS
#undef IT
    DetourTransactionCommit();
    g_d3dDevice = {};
    dprintf("Cleaned up Detours.");
  }
  return TRUE;
}
