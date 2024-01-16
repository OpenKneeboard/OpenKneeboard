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

#include "OpenXRD3D12Kneeboard.h"

#include "OpenXRD3D11Kneeboard.h"
#include "OpenXRNext.h"

#include <OpenKneeboard/D3D11.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <string>

#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>

#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {

OpenXRD3D12Kneeboard::OpenXRD3D12Kneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D12KHR& binding)
  : OpenXRKneeboard(session, runtimeID, next), mSHM(binding.device) {
  dprintf("{}", __FUNCTION__);

  mDeviceResources
    = std::make_unique<DeviceResources>(binding.device, binding.queue);
}

OpenXRD3D12Kneeboard::~OpenXRD3D12Kneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRD3D12Kneeboard()");
}

bool OpenXRD3D12Kneeboard::ConfigurationsAreCompatible(
  const VRRenderConfig& initial,
  const VRRenderConfig& current) const {
  return true;
}

XrSwapchain OpenXRD3D12Kneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size,
  const VRRenderConfig::Quirks& quirks) {
  dprintf("{}", __FUNCTION__);

  auto oxr = this->GetOpenXR();

  auto formats = OpenXRD3D11Kneeboard::GetDXGIFormats(oxr, session);
  dprintf(
    "Creating swapchain with format {}",
    static_cast<INT>(formats.mTextureFormat));

  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
    .format = formats.mTextureFormat,
    .sampleCount = 1,
    .width = size.mWidth,
    .height = size.mHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };

  XrSwapchain swapchain {nullptr};

  auto nextResult = oxr->xrCreateSwapchain(session, &swapchainInfo, &swapchain);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to create swapchain: {}", nextResult);
    return nullptr;
  }

  uint32_t imageCount = 0;
  nextResult
    = oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || nextResult != XR_SUCCESS) {
    dprintf("No images in swapchain: {}", nextResult);
    return nullptr;
  }

  dprintf("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageD3D12KHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageD3D12KHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR,
    });

  nextResult = oxr->xrEnumerateSwapchainImages(
    swapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to enumerate images in swapchain: {}", nextResult);
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
    dprint("Swap chain is not a D3D12 swapchain");
    OPENKNEEBOARD_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  auto& resources = mSwapchainResources[swapchain];
  resources = SwapchainResources {
    .mViewport = {
      0.0f,
      0.0f,
      static_cast<FLOAT>(size.mWidth),
      static_cast<FLOAT>(size.mHeight),
      0.0f,
      1.0f,
    },
    .mScissorRect = {
      0,
      0,
      static_cast<LONG>(size.mWidth),
      static_cast<LONG>(size.mHeight),
    },
    .mRenderTargetViewFormat = formats.mRenderTargetViewFormat,
  };

  auto dr = mDeviceResources.get();
  resources.mD3D12RenderTargetViewsHeap
    = std::make_unique<DirectX::DescriptorHeap>(
      dr->mD3D12Device.get(),
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
      D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
      imageCount);

  for (uint32_t i = 0; i < imageCount; ++i) {
    winrt::com_ptr<ID3D12Resource> texture;
    texture.copy_from(images.at(i).texture);

    D3D12_RENDER_TARGET_VIEW_DESC desc {
      .Format = resources.mRenderTargetViewFormat,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {
        .MipSlice = 0,
        .PlaneSlice = 0,
      },
    };
    dr->mD3D12Device->CreateRenderTargetView(
      texture.get(),
      &desc,
      resources.mD3D12RenderTargetViewsHeap->GetCpuHandle(i));

    resources.mD3D12Textures.push_back(std::move(texture));
  }

  {
    DirectX::ResourceUploadBatch resourceUpload(dr->mD3D12Device.get());
    resourceUpload.Begin();
    DirectX::RenderTargetState rtState(
      formats.mRenderTargetViewFormat, DXGI_FORMAT_D32_FLOAT);
    DirectX::SpriteBatchPipelineStateDescription pd(rtState);
    resources.mDXTK12SpriteBatch = std::make_unique<DirectX::SpriteBatch>(
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

    const auto colorDesc = images.at(0).texture->GetDesc();

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
      IID_PPV_ARGS(resources.mD3D12DepthStencilTexture.put())));
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
      resources.mD3D12DepthStencilTexture.get(),
      &desc,
      dr->mD3D12DepthHeap->GetFirstCpuHandle());
  }

  return swapchain;
}

void OpenXRD3D12Kneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!mSwapchainResources.contains(swapchain)) {
    return;
  }
  mSwapchainResources.erase(swapchain);
}

bool OpenXRD3D12Kneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  uint8_t layerCount,
  LayerRenderInfo* layers) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "OpenXRD3D12Kneeboard::RenderLayers()");

  auto dr = mDeviceResources.get();
  auto& sr = mSwapchainResources[swapchain];

  TraceLoggingWriteTagged(activity, "SignalD3D11Fence");
  const auto fenceValueD3D11Finished = ++dr->mFenceValue;
  winrt::check_hresult(dr->mD3D11ImmediateContext->Signal(
    dr->mD3D11Fence.get(), fenceValueD3D11Finished));

  TraceLoggingWriteTagged(activity, "InitD3D12Frame");
  auto sprites = sr.mDXTK12SpriteBatch.get();
  sprites->SetViewport(sr.mViewport);

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  dr->mD3D12CommandAllocator->Reset();
  winrt::check_hresult(dr->mD3D12Device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    dr->mD3D12CommandAllocator.get(),
    nullptr,
    IID_PPV_ARGS(commandList.put())));
  commandList->RSSetViewports(1, &sr.mViewport);
  commandList->RSSetScissorRects(1, &sr.mScissorRect);

  auto rt = sr.mD3D12RenderTargetViewsHeap->GetCpuHandle(swapchainTextureIndex);
  auto depthStencil = dr->mD3D12DepthHeap->GetFirstCpuHandle();
  commandList->ClearDepthStencilView(
    depthStencil, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  commandList->OMSetRenderTargets(1, &rt, true, &depthStencil);

  auto heap = mSHM.GetShaderResourceViewHeap();
  commandList->SetDescriptorHeaps(1, &heap);

  {
    TraceLoggingThreadActivity<gTraceProvider> spritesActivity;
    TraceLoggingWriteStart(spritesActivity, "SpriteBatch");
    sprites->Begin(commandList.get());
    for (uint8_t i = 0; i < layerCount; ++i) {
      const auto& info = layers[i];
      auto resources
        = snapshot.GetLayerGPUResources<SHM::D3D12::LayerTextureCache>(i);
      auto srv = resources->GetD3D12ShaderResourceViewGPUHandle();

      const auto opacity = info.mVR.mKneeboardOpacity;
      DirectX::FXMVECTOR tint {opacity, opacity, opacity, opacity};

      RECT sourceRect = info.mSourceRect;
      RECT destRect = info.mDestRect;
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

  TraceLoggingWriteStop(activity, "OpenXRD3D12Kneeboard::RenderLayers()");
  return true;
}

winrt::com_ptr<ID3D11Device> OpenXRD3D12Kneeboard::GetD3D11Device() {
  return mDeviceResources->mD3D11Device;
}

SHM::CachedReader* OpenXRD3D12Kneeboard::GetSHM() {
  return &mSHM;
}

}// namespace OpenKneeboard
