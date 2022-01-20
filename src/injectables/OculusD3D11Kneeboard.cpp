#include "OculusD3D11Kneeboard.h"

#include <OVR_CAPI_D3D.h>
#include <OpenKneeboard/dprint.h>

#include "InjectedDLLMain.h"
#include "OVRProxy.h"

namespace OpenKneeboard {

OculusD3D11Kneeboard::OculusD3D11Kneeboard() {
  dprintf("{}, {:#018x}", __FUNCTION__, (uint64_t)this);
  mOculusKneeboard = std::make_unique<OculusKneeboard>(this);
  mDXGIHook.InstallHook({
    .onPresent
    = std::bind_front(&OculusD3D11Kneeboard::OnIDXGISwapChain_Present, this),
  });
}

OculusD3D11Kneeboard::~OculusD3D11Kneeboard() {
  dprintf("{}, {:#018x}", __FUNCTION__, (uint64_t)this);
  this->UninstallHook();
}

void OculusD3D11Kneeboard::UninstallHook() {
  mOculusKneeboard->UninstallHook();
  mDXGIHook.UninstallHook();
}

ovrTextureSwapChain OculusD3D11Kneeboard::CreateSwapChain(
  ovrSession session,
  const SHM::Config& config) {
  auto ovr = OVRProxy::Get();
  ovrTextureSwapChain swapChain = nullptr;

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
    session, mD3D.get(), &kneeboardSCD, &swapChain);
  if (!swapChain) {
    return nullptr;
  }

  int length = -1;
  ovr->ovr_GetTextureSwapChainLength(session, swapChain, &length);

  mRenderTargets.clear();
  mRenderTargets.resize(length);
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D11Texture2D> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, swapChain, i, IID_PPV_ARGS(&texture));

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

  return swapChain;
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

  static_assert(SHM::Pixel::IS_PREMULTIPLIED_B8G8R8A8);

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

  mDXGIHook.UninstallHook();
  return std::invoke(next, swapChain, syncInterval, flags);
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

namespace {
std::unique_ptr<OculusD3D11Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<OculusD3D11Kneeboard>();
  dprintf(
    FMT_STRING("----- OculusD3D11Kneeboard active at {:#018x} -----"),
    (intptr_t)gInstance.get());
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
