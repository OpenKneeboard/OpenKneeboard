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

ID3D12DescriptorHeap* CachedReader::GetShaderResourceViewHeap() {
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
  winrt::check_hresult(mD3D12Device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    IID_PPV_ARGS(mD3D12CommandAllocator.put())));

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

  for (size_t i = 0; i < textureCount; ++i) {
    winrt::com_ptr<ID3D12Resource> texture;
    texture.copy_from(textures[i]);

    D3D12_RENDER_TARGET_VIEW_DESC desc {
      .Format = renderTargetViewFormat,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {
        .MipSlice = 0,
        .PlaneSlice = 0,
      },
    };
    dr->mD3D12Device->CreateRenderTargetView(
      texture.get(), &desc, mD3D12RenderTargetViewsHeap->GetCpuHandle(i));

    mD3D12Textures.push_back(std::move(texture));
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

}// namespace Renderer

}// namespace OpenKneeboard::SHM::D3D12