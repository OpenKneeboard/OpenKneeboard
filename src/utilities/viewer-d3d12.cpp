// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "viewer-d3d12.hpp"

#include <OpenKneeboard/D3D12.hpp>
#include <OpenKneeboard/RenderDoc.hpp>

#include <OpenKneeboard/hresult.hpp>

#include <directxtk12/ScreenGrab.h>

namespace OpenKneeboard::Viewer {

D3D12Renderer::D3D12Renderer(IDXGIAdapter* dxgiAdapter) {
  dprint(__FUNCSIG__);

#ifdef DEBUG
  dprint("Enabling D3D12 debug features");
  winrt::com_ptr<ID3D12Debug5> debug;
  check_hresult(D3D12GetDebugInterface(IID_PPV_ARGS(debug.put())));
  debug->EnableDebugLayer();
#endif

  mDevice.capture(D3D12CreateDevice, dxgiAdapter, D3D_FEATURE_LEVEL_12_1);

#ifdef DEBUG
  if (!RenderDoc::IsPresent()) {
    // There's also the semi-documented ID3D12InfoQueue1 interface if we end up
    // wanting a full callback
    const auto infoQueue = mDevice.try_as<ID3D12InfoQueue>();
    if (infoQueue) {
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    }
  }
#endif

  D3D12_COMMAND_QUEUE_DESC queueDesc {D3D12_COMMAND_LIST_TYPE_DIRECT};
  mCommandQueue.capture(mDevice, &ID3D12Device::CreateCommandQueue, &queueDesc);
  mCommandAllocator.capture(
    mDevice,
    &ID3D12Device::CreateCommandAllocator,
    D3D12_COMMAND_LIST_TYPE_DIRECT);

  mGraphicsMemory = std::make_unique<DirectX::GraphicsMemory>(mDevice.get());

  mSpriteBatch = std::make_unique<D3D12::SpriteBatch>(
    mDevice.get(), mCommandQueue.get(), DXGI_FORMAT_B8G8R8A8_UNORM);

  mDestRTVHeap = std::make_unique<DirectX::DescriptorHeap>(
    mDevice.get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    1);

  mSHM = std::make_unique<SHM::D3D12::Reader>(
    SHM::ConsumerKind::Viewer, mDevice.get(), mCommandQueue.get());
}

D3D12Renderer::~D3D12Renderer() = default;

std::wstring_view D3D12Renderer::GetName() const noexcept {
  return {L"D3D12"};
}

SHM::Reader& D3D12Renderer::GetSHM() {
  return *mSHM;
}

void D3D12Renderer::Initialize(uint8_t swapchainLength) {
}

uint64_t D3D12Renderer::Render(
  SHM::Frame rawSource,
  const PixelRect& sourceRect,
  HANDLE destTextureHandle,
  const PixelSize& destTextureDimensions,
  const PixelRect& destRect,
  HANDLE fenceHandle,
  uint64_t fenceValueIn) {
  OPENKNEEBOARD_TraceLoggingScope("Viewer::D3D12Renderer::Render");

  const auto source = mSHM->Map(std::move(rawSource));

  RenderDoc::NestedFrameCapture renderDocFrame(
    mDevice.get(), "D3D12Renderer::Render()");

  if (mDestDimensions != destTextureDimensions) {
    mDestHandle = {};
  }
  if (destTextureHandle != mDestHandle) {
    mDestTexture = nullptr;
    mDestTexture.capture(
      mDevice, &ID3D12Device::OpenSharedHandle, destTextureHandle);
    mDestHandle = destTextureHandle;
    mDestDimensions = destTextureDimensions;
  }

  if (fenceHandle != mFenceHandle) {
    mFence.capture(mDevice, &ID3D12Device::OpenSharedHandle, fenceHandle);
    mFenceHandle = fenceHandle;
  }

  auto dest = mDestRTVHeap->GetFirstCpuHandle();
  mDevice->CreateRenderTargetView(mDestTexture.get(), nullptr, dest);

  if (!mCommandList) {
    mCommandList.capture(
      mDevice,
      &ID3D12Device::CreateCommandList,
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      mCommandAllocator.get(),
      nullptr);
  }

  auto cl = mCommandList.get();

  ID3D12DescriptorHeap* heaps[] {source.mShaderResourceViewHeap};
  cl->SetDescriptorHeaps(std::size(heaps), heaps);

  D3D12_RESOURCE_BARRIER inBarriers[] {
    D3D12_RESOURCE_BARRIER {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
        .pResource = mDestTexture.get(),
        .StateBefore = D3D12_RESOURCE_STATE_COMMON,
        .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
      },
    },
  };
  cl->ResourceBarrier(std::size(inBarriers), inBarriers);

  mSpriteBatch->Begin(cl, dest, destTextureDimensions);
  mSpriteBatch->Draw(
    source.mTexture, source.mTextureDimensions, sourceRect, destRect);
  mSpriteBatch->End();

  D3D12_RESOURCE_BARRIER outBarriers[] {
    D3D12_RESOURCE_BARRIER {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
        .pResource = mDestTexture.get(),
        .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
        .StateAfter = D3D12_RESOURCE_STATE_COMMON,
      },
    },
  };
  cl->ResourceBarrier(std::size(outBarriers), outBarriers);

  check_hresult(cl->Close());

  check_hresult(mCommandQueue->Wait(mFence.get(), fenceValueIn));

  ID3D12CommandList* lists[] {cl};
  mCommandQueue->ExecuteCommandLists(std::size(lists), lists);

  const auto fenceValueOut = fenceValueIn + 1;
  check_hresult(mCommandQueue->Signal(mFence.get(), fenceValueOut));

  {
    OPENKNEEBOARD_TraceLoggingScope("ResetCommandList");
    check_hresult(cl->Reset(mCommandAllocator.get(), nullptr));
  }

  return fenceValueOut;
}

void D3D12Renderer::SaveToDDSFile(
  SHM::Frame raw,
  const std::filesystem::path& path) {
  const auto frame = mSHM->Map(std::move(raw));
  check_hresult(
    DirectX::SaveDDSTextureToFile(
      mCommandQueue.get(), frame.mTexture, path.wstring().c_str()));
}

}// namespace OpenKneeboard::Viewer