#include "OculusD3D11Kneeboard.h"

#include <OVR_CAPI_D3D.h>
#include <OpenKneeboard/dprint.h>

#include "InjectedDLLMain.h"
#include "OVRProxy.h"

namespace OpenKneeboard {

OculusD3D11Kneeboard::OculusD3D11Kneeboard() {
  dprintf("{}, {:#018x}", __FUNCTION__, (uint64_t)this);
  IDXGISwapChainPresentHook::InitWithVTable();
  mOculusKneeboard = std::make_unique<OculusKneeboard>(this);
}

OculusD3D11Kneeboard::~OculusD3D11Kneeboard() {
  dprintf("{}, {:#018x}", __FUNCTION__, (uint64_t)this);
  this->UninstallHook();
}

void OculusD3D11Kneeboard::UninstallHook() {
  mOculusKneeboard->UninstallHook();
  IDXGISwapChainPresentHook::UninstallHook();
}

ovrTextureSwapChain OculusD3D11Kneeboard::GetSwapChain(
  ovrSession session,
  const SHM::Config& config) {
  auto ovr = OVRProxy::Get();
  if (mSwapChain) {
    const auto& prev = mLastConfig;
    if (
      config.imageWidth == prev.imageWidth
      && config.imageHeight == prev.imageHeight) {
      return mSwapChain;
    }
    ovr->ovr_DestroyTextureSwapChain(session, mSwapChain);
    mSwapChain = nullptr;
  }

  if (!mD3D) {
    return nullptr;
  }

  mLastConfig = config;

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

  ovr->ovr_CreateTextureSwapChainDX(
    session, mD3D.get(), &kneeboardSCD, &mSwapChain);
  if (!mSwapChain) {
    return nullptr;
  }

  int length = -1;
  ovr->ovr_GetTextureSwapChainLength(session, mSwapChain, &length);

  mRenderTargets.clear();
  mRenderTargets.resize(length);
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D11Texture2D> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, mSwapChain, i, IID_PPV_ARGS(&texture));

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MipSlice = 0}};

    auto hr = mD3D->CreateRenderTargetView(
      texture.get(), &rtvd, mRenderTargets.at(i).put());
    if (FAILED(hr)) {
      dprintf(" - failed to create render target view");
      return nullptr;
    }
  }

  return mSwapChain;
}

bool OculusD3D11Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot) {
  if (!swapChain) {
    return false;
  }

  if (!mD3D) {
    dprintf(" - no D3D - {}", __FUNCTION__);
    return false;
  }

  auto ovr = OVRProxy::Get();
  auto& config = *snapshot.GetConfig();

  int index = -1;
  ovr->ovr_GetTextureSwapChainCurrentIndex(session, swapChain, &index);
  if (index < 0) {
    dprintf(" - invalid swap chain index ({})", index);
    return false;
  }

  winrt::com_ptr<ID3D11Texture2D> texture;
  ovr->ovr_GetTextureSwapChainBufferDX(
    session, swapChain, index, IID_PPV_ARGS(&texture));
  winrt::com_ptr<ID3D11DeviceContext> context;
  mD3D->GetImmediateContext(context.put());
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

  auto ret = ovr->ovr_CommitTextureSwapChain(session, swapChain);
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
  if (!mD3D) {
    swapChain->GetDevice(IID_PPV_ARGS(mD3D.put()));
    if (!mD3D) {
      dprintf("Got a swapchain without a D3D11 device");
    }
  }

  auto ret = std::invoke(next, swapChain, syncInterval, flags);
  IDXGISwapChainPresentHook::UninstallHook();
  return ret;
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

namespace {
std::unique_ptr<OculusD3D11Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<OculusD3D11Kneeboard>();
  dprintf(
    FMT_STRING("Kneeboard active at {:#018x}"), (intptr_t)gInstance.get());
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
