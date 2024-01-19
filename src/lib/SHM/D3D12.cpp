/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/SHM/D3D12.h>

#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

namespace OpenKneeboard::SHM::D3D12 {

struct DeviceResources {
  winrt::com_ptr<ID3D12Device> mDevice;
  DirectX::DescriptorHeap mSRVHeap;
};

LayerTextureCache::LayerTextureCache(
  uint8_t layerIndex,
  const winrt::com_ptr<ID3D11Texture2D>& d3d11Texture,
  const std::shared_ptr<DeviceResources>& resources)
  : SHM::GFXInterop::LayerTextureCache(d3d11Texture),
    mLayerIndex(layerIndex),
    mDeviceResources(resources) {
}

ID3D12Resource* LayerTextureCache::GetD3D12Texture() {
  if (!mD3D12Texture) [[unlikely]] {
    winrt::check_hresult(mDeviceResources->mDevice->OpenSharedHandle(
      this->GetNTHandle(), IID_PPV_ARGS(mD3D12Texture.put())));
  }
  return mD3D12Texture.get();
}

D3D12_GPU_DESCRIPTOR_HANDLE
LayerTextureCache::GetD3D12ShaderResourceViewGPUHandle() {
  if (!mHaveShaderResourceView) [[unlikely]] {
    mDeviceResources->mDevice->CreateShaderResourceView(
      this->GetD3D12Texture(),
      nullptr,
      mDeviceResources->mSRVHeap.GetCpuHandle(mLayerIndex));
    mHaveShaderResourceView = true;
  }

  return mDeviceResources->mSRVHeap.GetGpuHandle(mLayerIndex);
}

LayerTextureCache::~LayerTextureCache() = default;

CachedReader::CachedReader(ID3D12Device* devicePtr) {
  winrt::com_ptr<ID3D12Device> device;
  device.copy_from(devicePtr);

  DirectX::DescriptorHeap srvHeap(device.get(), MaxLayers);

  mDeviceResources
    = std::make_shared<DeviceResources>(std::move(device), std::move(srvHeap));
}

CachedReader::~CachedReader() = default;

ID3D12DescriptorHeap* CachedReader::GetShaderResourceViewHeap() const {
  return mDeviceResources->mSRVHeap.Heap();
}

std::shared_ptr<SHM::LayerTextureCache> CachedReader::CreateLayerTextureCache(
  uint8_t layerIndex,
  const winrt::com_ptr<ID3D11Texture2D>& texture) {
  return std::make_shared<SHM::D3D12::LayerTextureCache>(
    layerIndex, texture, mDeviceResources);
}

namespace Renderer {

DeviceResources::DeviceResources(
  ID3D12Device* device,
  ID3D12CommandQueue* queue) {
  mD3D12Device.copy_from(device);
  mD3D12CommandQueue.copy_from(queue);

  UINT flags = 0;
#ifdef DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  winrt::com_ptr<ID3D11Device> device11;
  winrt::com_ptr<ID3D11DeviceContext> context11;
  D3D11On12CreateDevice(
    mD3D12Device.get(),
    flags,
    nullptr,
    0,
    nullptr,
    0,
    1,
    device11.put(),
    context11.put(),
    nullptr);
  mD3D11Device = device11.as<ID3D11Device5>();
  mD3D11ImmediateContext = context11.as<ID3D11DeviceContext4>();
  mD3D11On12Device = device11.as<ID3D11On12Device>();

  mD3D12DepthHeap = std::make_unique<DirectX::DescriptorHeap>(
    mD3D12Device.get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    1);

  mDXTK12GraphicsMemory
    = std::make_unique<DirectX::GraphicsMemory>(mD3D12Device.get());

  mD3D12Device->CreateFence(
    0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(mD3D12Fence.put()));
  mFenceEvent.attach(CreateEventEx(nullptr, nullptr, 0, GENERIC_ALL));
  winrt::check_hresult(mD3D12Device->CreateSharedHandle(
    mD3D12Fence.get(),
    nullptr,
    GENERIC_ALL,
    nullptr,
    mD3DInteropFenceHandle.put()));
  winrt::check_hresult(mD3D11Device->OpenSharedFence(
    mD3DInteropFenceHandle.get(), IID_PPV_ARGS(mD3D11Fence.put())));
}

SwapchainResources::SwapchainResources(
  DeviceResources* dr,
  DXGI_FORMAT renderTargetViewFormat,
  size_t textureCount,
  ID3D12Resource** textures) {
  if (textureCount == 0) [[unlikely]] {
    dprint("ERROR: Asked to create swapchain resources with 0 textures!");
    OPENKNEEBOARD_BREAK;
    return;
  }

  const auto colorDesc = textures[0]->GetDesc();

  mViewport = {
    0.0f,
    0.0f,
    static_cast<FLOAT>(colorDesc.Width),
    static_cast<FLOAT>(colorDesc.Height),
    0.0f,
    1.0f,
  };
  mScissorRect = {
    0,
    0,
    static_cast<LONG>(colorDesc.Width),
    static_cast<LONG>(colorDesc.Height),
  };

  mD3D12RenderTargetViewsHeap = std::make_unique<DirectX::DescriptorHeap>(
    dr->mD3D12Device.get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    textureCount);

  mBufferResources.reserve(textureCount);
  for (size_t i = 0; i < textureCount; ++i) {
    auto br = std::make_unique<BufferResources>();

    br->mD3D12Texture.copy_from(textures[i]);
    br->mD3D12RenderTargetView = mD3D12RenderTargetViewsHeap->GetCpuHandle(i);

    D3D12_RENDER_TARGET_VIEW_DESC desc {
      .Format = renderTargetViewFormat,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {
        .MipSlice = 0,
        .PlaneSlice = 0,
      },
    };
    dr->mD3D12Device->CreateRenderTargetView(
      textures[i], &desc, br->mD3D12RenderTargetView);

    winrt::check_hresult(dr->mD3D12Device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(br->mD3D12CommandAllocator.put())));

    mBufferResources.push_back(std::move(br));
  }

  {
    DirectX::ResourceUploadBatch resourceUpload(dr->mD3D12Device.get());
    resourceUpload.Begin();
    DirectX::RenderTargetState rtState(
      renderTargetViewFormat, DXGI_FORMAT_D32_FLOAT);
    DirectX::SpriteBatchPipelineStateDescription pd(rtState);
    mDXTK12SpriteBatch = std::make_unique<DirectX::SpriteBatch>(
      dr->mD3D12Device.get(), resourceUpload, pd);
    auto uploadResourcesFinished
      = resourceUpload.End(dr->mD3D12CommandQueue.get());
    uploadResourcesFinished.wait();
  }

  {
    D3D12_HEAP_PROPERTIES heap {
      .Type = D3D12_HEAP_TYPE_DEFAULT,
      .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
      .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
    };

    D3D12_RESOURCE_DESC desc {
      .Dimension = colorDesc.Dimension,
      .Alignment = colorDesc.Alignment,
      .Width = colorDesc.Width,
      .Height = colorDesc.Height,
      .DepthOrArraySize = colorDesc.DepthOrArraySize,
      .MipLevels = 1,
      .Format = DXGI_FORMAT_R32_TYPELESS,
      .SampleDesc = {
        .Count = 1,
      },
      .Layout = colorDesc.Layout,
      .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
    };

    D3D12_CLEAR_VALUE clearValue {
      .Format = DXGI_FORMAT_D32_FLOAT,
      .DepthStencil = {
        .Depth = 1.0f,
      },
    };

    winrt::check_hresult(dr->mD3D12Device->CreateCommittedResource(
      &heap,
      D3D12_HEAP_FLAG_NONE,
      &desc,
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      &clearValue,
      IID_PPV_ARGS(mD3D12DepthStencilTexture.put())));
  }
  {
    D3D12_DEPTH_STENCIL_VIEW_DESC desc {
      .Format = DXGI_FORMAT_D32_FLOAT,
      .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
      .Flags = D3D12_DSV_FLAG_NONE,
      .Texture2D = D3D12_TEX2D_DSV {
        .MipSlice = 0, 
      },
    };
    dr->mD3D12Device->CreateDepthStencilView(
      mD3D12DepthStencilTexture.get(),
      &desc,
      dr->mD3D12DepthHeap->GetFirstCpuHandle());
  }
}

void ClearRenderTargetView(
  DeviceResources* dr,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "OpenKneeboard::SHM::D3D12::ClearRenderTargetView");

  auto br = sr->mBufferResources.at(swapchainTextureIndex).get();
  const auto& rtv = br->mD3D12RenderTargetView;

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  winrt::check_hresult(dr->mD3D12Device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    br->mD3D12CommandAllocator.get(),
    nullptr,
    IID_PPV_ARGS(commandList.put())));
  commandList->RSSetViewports(1, &sr->mViewport);
  commandList->RSSetScissorRects(1, &sr->mScissorRect);
  commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

  commandList->ClearRenderTargetView(
    rtv, DirectX::Colors::Transparent, 0, nullptr);
  winrt::check_hresult(commandList->Close());

  ID3D12CommandList* commandLists[] {commandList.get()};
  dr->mD3D12CommandQueue->ExecuteCommandLists(1, commandLists);

  TraceLoggingWriteStop(
    activity, "OpenKneeboard::SHM::D3D12::ClearRenderTargetView");
}

void Render(
  DeviceResources* dr,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex,
  const SHM::D3D12::CachedReader& shm,
  const SHM::Snapshot& snapshot,
  size_t layerSpriteCount,
  LayerSprite* layerSprites) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "OpenKneeboard::SHM::D3D12::Renderer::Render");

  auto br = sr->mBufferResources.at(swapchainTextureIndex).get();

  TraceLoggingWriteTagged(activity, "SignalD3D11Fence");
  const auto fenceValueD3D11Finished = ++dr->mFenceValue;
  winrt::check_hresult(dr->mD3D11ImmediateContext->Signal(
    dr->mD3D11Fence.get(), fenceValueD3D11Finished));

  TraceLoggingWriteTagged(activity, "InitD3D12Frame");
  auto sprites = sr->mDXTK12SpriteBatch.get();
  sprites->SetViewport(sr->mViewport);

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  winrt::check_hresult(dr->mD3D12Device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    br->mD3D12CommandAllocator.get(),
    nullptr,
    IID_PPV_ARGS(commandList.put())));
  commandList->RSSetViewports(1, &sr->mViewport);
  commandList->RSSetScissorRects(1, &sr->mScissorRect);

  const auto& rtv = br->mD3D12RenderTargetView;
  auto depthStencil = dr->mD3D12DepthHeap->GetFirstCpuHandle();
  commandList->ClearDepthStencilView(
    depthStencil, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  commandList->OMSetRenderTargets(1, &rtv, true, &depthStencil);
  auto srvHeap = shm.GetShaderResourceViewHeap();
  commandList->SetDescriptorHeaps(1, &srvHeap);

  {
    TraceLoggingThreadActivity<gTraceProvider> spritesActivity;
    TraceLoggingWriteStart(spritesActivity, "SpriteBatch");
    sprites->Begin(commandList.get());
    for (uint8_t i = 0; i < layerSpriteCount; ++i) {
      const auto& sprite = layerSprites[i];
      auto resources
        = snapshot.GetLayerGPUResources<SHM::D3D12::LayerTextureCache>(
          sprite.mLayerIndex);
      auto config = snapshot.GetLayerConfig(sprite.mLayerIndex);
      auto srv = resources->GetD3D12ShaderResourceViewGPUHandle();

      const auto opacity = sprite.mOpacity;
      DirectX::FXMVECTOR tint {opacity, opacity, opacity, opacity};

      const D3D12_RECT sourceRect = config->mLocationOnTexture;
      const D3D11_RECT destRect = sprite.mDestRect;

      sprites->Draw(
        srv, {TextureWidth, TextureHeight}, destRect, &sourceRect, tint);
    }
    TraceLoggingWriteTagged(spritesActivity, "SpriteBatch::End");
    sprites->End();
    TraceLoggingWriteStop(spritesActivity, "SpriteBatch");
  }
  TraceLoggingWriteTagged(activity, "CloseCommandList");
  winrt::check_hresult(commandList->Close());

  TraceLoggingWriteTagged(activity, "WaitD3D12Fence");
  winrt::check_hresult(dr->mD3D12CommandQueue->Wait(
    dr->mD3D12Fence.get(), fenceValueD3D11Finished));
  dr->mD3D11ImmediateContext->Flush();

  TraceLoggingWriteTagged(activity, "ExecuteCommandLists");
  {
    ID3D12CommandList* commandLists[] = {commandList.get()};
    dr->mD3D12CommandQueue->ExecuteCommandLists(1, commandLists);
  }

  TraceLoggingWriteStop(
    activity, "OpenKneeboard::SHM::D3D12::Renderer::Render");
}

void BeginFrame(
  DeviceResources*,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex) {
  sr->mBufferResources.at(swapchainTextureIndex)
    ->mD3D12CommandAllocator->Reset();
}

void EndFrame(
  DeviceResources*,
  SwapchainResources*,
  [[maybe_unused]] uint8_t swapchainTextureIndex) {
}

}// namespace Renderer

}// namespace OpenKneeboard::SHM::D3D12