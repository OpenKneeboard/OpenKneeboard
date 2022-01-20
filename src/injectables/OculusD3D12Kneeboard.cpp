#include "OculusD3D12Kneeboard.h"

#include <OpenKneeboard/dprint.h>

#include "InjectedDLLMain.h"
#include "OVRProxy.h"

namespace OpenKneeboard {

OculusD3D12Kneeboard::OculusD3D12Kneeboard() {
  mExecuteCommandListsHook.InstallHook({
    .onExecuteCommandLists = std::bind_front(
      &OculusD3D12Kneeboard::OnID3D12CommandQueue_ExecuteCommandLists, this),
  });
  mOculusKneeboard = std::make_unique<OculusKneeboard>(this);
}

OculusD3D12Kneeboard::~OculusD3D12Kneeboard() {
  UninstallHook();
}

void OculusD3D12Kneeboard::UninstallHook() {
  mExecuteCommandListsHook.UninstallHook();
  mOculusKneeboard->UninstallHook();
}

ovrTextureSwapChain OculusD3D12Kneeboard::CreateSwapChain(
  ovrSession session,
  const SHM::Config& config) {
  if (!(mCommandQueue && mDevice)) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  ovrTextureSwapChain swapChain = nullptr;

  static_assert(SHM::Pixel::IS_PREMULTIPLIED_B8G8R8A8);
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

  auto ovr = OVRProxy::Get();

  ovr->ovr_CreateTextureSwapChainDX(
    session, mCommandQueue.get(), &kneeboardSCD, &swapChain);
  if (!swapChain) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  int length = -1;
  ovr->ovr_GetTextureSwapChainLength(session, swapChain, &length);
  if (length == -1) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  mDescriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC dhDesc {
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    .NumDescriptors = static_cast<UINT>(length),
    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    .NodeMask = 0,
  };
  mDevice->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(mDescriptorHeap.put()));
  if (!mDescriptorHeap) {
    dprintf("Failed to get descriptor heap: {:#x}", GetLastError());
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  auto heap = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
  auto increment = mDevice->GetDescriptorHandleIncrementSize(dhDesc.Type);

  for (auto i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D12Resource> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, swapChain, i, IID_PPV_ARGS(texture.put()));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {},
    };

    auto dest = heap;
    dest.ptr += (i * increment);
    mDevice->CreateRenderTargetView(texture.get(), &rtvDesc, dest);
  }

  return swapChain;
}

bool OculusD3D12Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot) {
  auto ovr = OVRProxy::Get();
  const auto& config = *snapshot.GetConfig();

  int index = -1;
  ovr->ovr_GetTextureSwapChainCurrentIndex(session, swapChain, &index);
  if (index < 0) {
    dprintf(" - invalid swap chain index ({})", index);
    OPENKNEEBOARD_BREAK;
    return false;
  }

  winrt::com_ptr<ID3D12Resource> texture;
  ovr->ovr_GetTextureSwapChainBufferDX(
    session, swapChain, index, IID_PPV_ARGS(texture.put()));
  if (!texture) {
    OPENKNEEBOARD_BREAK;
    return false;
  }

  void* textureData = nullptr;
  texture->Map(0, nullptr, &textureData);
  memcpy(
    textureData,
    snapshot.GetPixels(),
    sizeof(SHM::Pixel) * config.imageHeight * config.imageWidth);
  texture->Unmap(0, nullptr);

  return true;
}

void OculusD3D12Kneeboard::OnID3D12CommandQueue_ExecuteCommandLists(
  ID3D12CommandQueue* this_,
  UINT NumCommandLists,
  ID3D12CommandList* const* ppCommandLists,
  const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next) {
  if (!mCommandQueue) {
    mCommandQueue.attach(this_);
    mCommandQueue->AddRef();
    this_->GetDevice(IID_PPV_ARGS(mDevice.put()));
    if (mCommandQueue && mDevice) {
      dprint("Got a D3D12 device and command queue");
    } else {
      OPENKNEEBOARD_BREAK;
    }
  }

  mExecuteCommandListsHook.UninstallHook();
  return std::invoke(next, this_, NumCommandLists, ppCommandLists);
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

namespace {
std::unique_ptr<OculusD3D12Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<OculusD3D12Kneeboard>();
  dprintf(
    FMT_STRING("----- OculusD3D12Kneeboard active at {:#018x} -----"),
    (intptr_t)gInstance.get());
  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-Oculus-D3D12",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
