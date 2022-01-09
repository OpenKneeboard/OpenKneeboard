#include "OculusD3D11Kneeboard.h"

#include <OVR_CAPI_D3D.h>
#include <d3d11.h>
#include <detours.h>

#include "D3D11DeviceHook.h"
#include "OpenKneeboard/dprint.h"

// Used, but not hooked
#define REAL_OVR_FUNCS \
  IT(ovr_CreateTextureSwapChainDX) \
  IT(ovr_GetTextureSwapChainLength) \
  IT(ovr_GetTextureSwapChainBufferDX) \
  IT(ovr_GetTextureSwapChainCurrentIndex) \
  IT(ovr_CommitTextureSwapChain) \
  IT(ovr_DestroyTextureSwapChain)

namespace OpenKneeboard {

#define IT(x) static decltype(&x) real_##x = nullptr;
REAL_OVR_FUNCS
#undef IT

class OculusD3D11Kneeboard::Impl final {
 public:
  SHM::Header Header;
  std::vector<winrt::com_ptr<ID3D11RenderTargetView>> RenderTargets;
  ovrTextureSwapChain SwapChain = nullptr;
  std::unique_ptr<D3D11DeviceHook> D3D;
};

OculusD3D11Kneeboard::OculusD3D11Kneeboard() : p(std::make_unique<Impl>()) {
  dprintf("{}", __FUNCTION__);
  p->D3D = std::make_unique<D3D11DeviceHook>();

#define IT(x) \
  real_##x = reinterpret_cast<decltype(&x)>( \
    DetourFindFunction("LibOVRRT64_1.dll", #x));
  REAL_OVR_FUNCS
#undef IT
}

OculusD3D11Kneeboard::~OculusD3D11Kneeboard() {
}

void OculusD3D11Kneeboard::Unhook() {
  OculusKneeboard::Unhook();
  p->D3D->Unhook();
}

ovrTextureSwapChain OculusD3D11Kneeboard::GetSwapChain(
  ovrSession session,
  const SHM::Header& config) {

  if (p->SwapChain) {
    const auto& prev = p->Header;
    if (
      config.ImageWidth == prev.ImageWidth
      && config.ImageHeight == prev.ImageHeight) {
      return p->SwapChain;
    }
    real_ovr_DestroyTextureSwapChain(session, p->SwapChain);
    p->SwapChain = nullptr;
  }

  auto d3d = p->D3D->MaybeGet();
  if (!d3d) {
    return nullptr;
  }

  p->Header = config;

  ovrTextureSwapChainDesc kneeboardSCD = {
    .Type = ovrTexture_2D,
    .Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB,
    .ArraySize = 1,
    .Width = config.ImageWidth,
    .Height = config.ImageHeight,
    .MipLevels = 1,
    .SampleCount = 1,
    .StaticImage = false,
    .MiscFlags = ovrTextureMisc_AutoGenerateMips,
    .BindFlags = ovrTextureBind_DX_RenderTarget,
  };

  real_ovr_CreateTextureSwapChainDX(
    session, d3d.get(), &kneeboardSCD, &p->SwapChain);
  if (!p->SwapChain) {
    return nullptr;
  }

  int length = -1;
  real_ovr_GetTextureSwapChainLength(session, p->SwapChain, &length);

  p->RenderTargets.clear();
  p->RenderTargets.resize(length);
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D11Texture2D> texture;
    real_ovr_GetTextureSwapChainBufferDX(
      session, p->SwapChain, i, IID_PPV_ARGS(&texture));

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MipSlice = 0}};

    auto hr = d3d->CreateRenderTargetView(
      texture.get(), &rtvd, p->RenderTargets.at(i).put());
    if (FAILED(hr)) {
      dprintf(" - failed to create render target view");
      return nullptr;
    }
  }

  dprintf("{} completed", __FUNCTION__);
  return p->SwapChain;
}

bool OculusD3D11Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot) {
  if (!swapChain) {
    return false;
  }

  auto d3d = p->D3D->MaybeGet();
  if (!d3d) {
    dprintf(" - no D3d - {}", __FUNCTION__);
    return false;
  }

  auto& config = *snapshot.GetHeader();

  int index = -1;
  real_ovr_GetTextureSwapChainCurrentIndex(session, swapChain, &index);
  if (index < 0) {
    dprintf(" - invalid swap chain index ({})", index);
    return false;
  }

  winrt::com_ptr<ID3D11Texture2D> texture;
  real_ovr_GetTextureSwapChainBufferDX(
    session, swapChain, index, IID_PPV_ARGS(&texture));
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

  static_assert(sizeof(SHM::Pixel) == 4, "Expecting B8G8R8A8 for DirectX");
  static_assert(offsetof(SHM::Pixel, b) == 0, "Expected blue to be first byte");
  static_assert(offsetof(SHM::Pixel, a) == 3, "Expected alpha to be last byte");

  context->UpdateSubresource(
    texture.get(),
    0,
    &box,
    snapshot.GetPixels(),
    config.ImageWidth * sizeof(SHM::Pixel),
    0);

  auto ret = real_ovr_CommitTextureSwapChain(session, swapChain);
  if (ret) {
    dprintf("Commit failed with {}", ret);
  }

  return true;
}

}// namespace OpenKneeboard
