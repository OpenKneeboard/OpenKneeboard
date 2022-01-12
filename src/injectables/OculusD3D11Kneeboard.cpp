#include "OculusD3D11Kneeboard.h"

#include <OVR_CAPI_D3D.h>
#include <OpenKneeboard/dprint.h>
#include <d3d11.h>
#include <detours.h>
#include <winrt/base.h>

#include "InjectedDLLMain.h"

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
  SHM::Header header;
  std::vector<winrt::com_ptr<ID3D11RenderTargetView>> renderTargets;
  ovrTextureSwapChain swapChain = nullptr;
  winrt::com_ptr<ID3D11Device> d3d = nullptr;
};

OculusD3D11Kneeboard::OculusD3D11Kneeboard() : p(std::make_unique<Impl>()) {
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
  IDXGISwapChainPresentHook::Unhook();
}

ovrTextureSwapChain OculusD3D11Kneeboard::GetSwapChain(
  ovrSession session,
  const SHM::Header& config) {
  if (p->swapChain) {
    const auto& prev = p->header;
    if (
      config.imageWidth == prev.imageWidth
      && config.imageHeight == prev.imageHeight) {
      return p->swapChain;
    }
    real_ovr_DestroyTextureSwapChain(session, p->swapChain);
    p->swapChain = nullptr;
  }

  if (!p->d3d) {
    return nullptr;
  }

  p->header = config;

  ovrTextureSwapChainDesc kneeboardSCD = {
    .Type = ovrTexture_2D,
    .Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB,
    .ArraySize = 1,
    .Width = config.imageWidth,
    .Height = config.imageHeight,
    .MipLevels = 1,
    .SampleCount = 1,
    .StaticImage = false,
    .MiscFlags = ovrTextureMisc_AutoGenerateMips,
    .BindFlags = ovrTextureBind_DX_RenderTarget,
  };

  real_ovr_CreateTextureSwapChainDX(
    session, p->d3d.get(), &kneeboardSCD, &p->swapChain);
  if (!p->swapChain) {
    return nullptr;
  }

  int length = -1;
  real_ovr_GetTextureSwapChainLength(session, p->swapChain, &length);

  p->renderTargets.clear();
  p->renderTargets.resize(length);
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D11Texture2D> texture;
    real_ovr_GetTextureSwapChainBufferDX(
      session, p->swapChain, i, IID_PPV_ARGS(&texture));

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MipSlice = 0}};

    auto hr = p->d3d->CreateRenderTargetView(
      texture.get(), &rtvd, p->renderTargets.at(i).put());
    if (FAILED(hr)) {
      dprintf(" - failed to create render target view");
      return nullptr;
    }
  }

  dprintf("{} completed", __FUNCTION__);
  return p->swapChain;
}

bool OculusD3D11Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot) {
  if (!swapChain) {
    return false;
  }

  if (!p->d3d) {
    dprintf(" - no D3D - {}", __FUNCTION__);
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
  p->d3d->GetImmediateContext(context.put());
  D3D11_BOX box {
    .left = 0,
    .top = 0,
    .front = 0,
    .right = config.imageWidth,
    .bottom = config.imageHeight,
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
    config.imageWidth * sizeof(SHM::Pixel),
    0);

  auto ret = real_ovr_CommitTextureSwapChain(session, swapChain);
  if (ret) {
    dprintf("Commit failed with {}", ret);
  }

  return true;
}

HRESULT OculusD3D11Kneeboard::OnIDXGISwapChain_Present(
  IDXGISwapChain* swapChain,
  UINT syncInterval,
  UINT flags,
  const decltype(&IDXGISwapChain::Present)& next) {
  if (!p->d3d) {
    swapChain->GetDevice(IID_PPV_ARGS(p->d3d.put()));
    if (!p->d3d) {
      dprintf("Got a swapchain without a D3D11 device");
    }
  }
  auto ret = std::invoke(next, swapChain, syncInterval, flags);
  IDXGISwapChainPresentHook::UnhookAndCleanup();
  return ret;
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

namespace {
std::unique_ptr<OculusD3D11Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  DetourTransactionPushBegin();
  gInstance = std::make_unique<OculusD3D11Kneeboard>();
  DetourTransactionPopCommit();
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-Oculus-D3D11",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
