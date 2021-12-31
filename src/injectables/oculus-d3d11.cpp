
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
#include "OculusFrameHook.h"
#include "OpenKneeboard/SHM.h"
#include "OpenKneeboard/dprint.h"

using SHMHeader = OpenKneeboard::SHM::Header;
using SHMPixel = OpenKneeboard::SHM::Pixel;

using namespace OpenKneeboard;

static_assert(sizeof(SHMPixel) == 4, "Expecting R8G8B8A8 for DirectX");
static_assert(offsetof(SHMPixel, r) == 0, "Expected red to be first byte");
static_assert(offsetof(SHMPixel, a) == 3, "Expected alpha to be last byte");

static SHM::Reader g_SHM;
static std::unique_ptr<D3D11DeviceHook> g_d3dDevice;

#define UNHOOKED_OVR_FUNCS \
  /* We need the versions from the process, not whatever we had handy when \ \ \
   * \ \ \ building */ \
  IT(ovr_CreateTextureSwapChainDX) \
  IT(ovr_GetTextureSwapChainLength) \
  IT(ovr_GetTextureSwapChainBufferDX) \
  IT(ovr_GetTextureSwapChainCurrentIndex) \
  IT(ovr_CommitTextureSwapChain) \
  IT(ovr_DestroyTextureSwapChain)

#define IT(x) static decltype(&x) Real_##x = nullptr;
UNHOOKED_OVR_FUNCS
#undef IT

class OculusD3D11Kneeboard final : public OculusFrameHook {
 private:
  SHMHeader mHeader;
  std::vector<winrt::com_ptr<ID3D11RenderTargetView>> mRenderTargets;

 public:
  ovrTextureSwapChain SwapChain = nullptr;

  bool isCompatibleWith(const SHMHeader& header) const {
    return header.ImageWidth == mHeader.ImageWidth
      && header.ImageHeight == mHeader.ImageHeight;
  }

  void Initialize(ovrSession session, const SHMHeader& config) {
    if (SwapChain) {
      Real_ovr_DestroyTextureSwapChain(session, SwapChain);
      SwapChain = nullptr;
    }
    mHeader = config;

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

    auto d3d = g_d3dDevice->MaybeGet();

    Real_ovr_CreateTextureSwapChainDX(
      session, d3d.get(), &kneeboardSCD, &SwapChain);
    if (!SwapChain) {
      return;
    }

    int length = -1;
    Real_ovr_GetTextureSwapChainLength(session, SwapChain, &length);

    mRenderTargets.resize(length);
    for (int i = 0; i < length; ++i) {
      ID3D11Texture2D* texture;// todo: smart ptr?
      Real_ovr_GetTextureSwapChainBufferDX(
        session, SwapChain, i, IID_PPV_ARGS(&texture));

      D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
        .Texture2D = {.MipSlice = 0}};

      auto hr = d3d->CreateRenderTargetView(
        texture, &rtvd, mRenderTargets.at(i).put());
      if (FAILED(hr)) {
        dprintf(" - failed to create render target view");
        return;
      }
    }

    dprintf("{} completed", __FUNCTION__);
  }

  bool Render(ovrSession session, const SHM::Snapshot& snapshot) {
    auto& config = *snapshot.GetHeader();

    if (!(SwapChain && isCompatibleWith(config))) {
      Initialize(session, config);
    }

    if (!SwapChain) {
      return false;
    }

    auto d3d = g_d3dDevice->MaybeGet();
    if (!d3d) {
      return false;
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
    d3d->GetImmediateContext(context.put());
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

    return true;
  }

  virtual ovrResult onEndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    decltype(&ovr_EndFrame) next) override {
    auto snapshot = g_SHM.MaybeGet();
    auto d3d = g_d3dDevice->MaybeGet();
    if (!(d3d && snapshot && Render(session, snapshot))) {
      return next(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
    }

    const auto& config = *snapshot.GetHeader();

    ovrLayerQuad kneeboardLayer = {};
    kneeboardLayer.Header.Type = ovrLayerType_Quad;
    kneeboardLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
    if ((config.Flags & OpenKneeboard::Flags::HEADLOCKED)) {
      kneeboardLayer.Header.Flags |= ovrLayerFlag_HeadLocked;
    }
    kneeboardLayer.ColorTexture = SwapChain;
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

    return next(
      session,
      frameIndex,
      viewScaleDesc,
      newLayers.data(),
      static_cast<unsigned int>(newLayers.size()));
  }
};
static std::unique_ptr<OculusD3D11Kneeboard> g_renderer;

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
    g_d3dDevice = std::make_unique<D3D11DeviceHook>();
    g_renderer = std::make_unique<OculusD3D11Kneeboard>();
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
    g_renderer.reset();
    g_d3dDevice.reset();
    DetourTransactionCommit();
    dprintf("Cleaned up Detours.");
  }
  return TRUE;
}
