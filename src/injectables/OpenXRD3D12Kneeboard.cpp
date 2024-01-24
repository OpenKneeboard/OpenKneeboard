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
  : OpenXRKneeboard(session, runtimeID, next) {
  dprintf("{}", __FUNCTION__);

  mDevice.copy_from(binding.device);
  mCommandQueue.copy_from(binding.queue);
  mSpriteBatch = std::make_unique<D3D12::SpriteBatch>(
    mDevice.get(), mCommandQueue.get(), DXGI_FORMAT_B8G8R8A8_UNORM);
}

OpenXRD3D12Kneeboard::~OpenXRD3D12Kneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRD3D12Kneeboard()");
}

XrSwapchain OpenXRD3D12Kneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size) {
  dprintf("{}", __FUNCTION__);
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D12Kneeboard::CreateSwapchain");

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

  this->ReleaseSwapchainResources(swapchain);
  mSwapchainResources.emplace(
    swapchain,
    SwapchainResources {
      .mDimensions = size,
      .mRenderTargetViewHeap
      = {mDevice.get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, imageCount},
    });

  auto& sr = mSwapchainResources.at(swapchain);
  for (const auto& image: images) {
    const auto imageIndex = sr.mBufferResources.size();
    sr.mBufferResources.push_back(
      {mDevice.get(),
       image.texture,
       sr.mRenderTargetViewHeap.GetCpuHandle(imageIndex),
       formats.mRenderTargetViewFormat});
  }

  return swapchain;
}

void OpenXRD3D12Kneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!mSwapchainResources.contains(swapchain)) {
    return;
  }
  mSwapchainResources.erase(swapchain);
}

void OpenXRD3D12Kneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  const PixelRect* const destRects,
  const float* const opacities) {
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D12Kneeboard::RenderLayers()");

  auto source = snapshot.GetTexture<SHM::D3D12::Texture>();

  const auto& sr = mSwapchainResources.at(swapchain);
  const auto& br = sr.mBufferResources.at(swapchainTextureIndex);

  mSpriteBatch->Begin(
    br.mCommandList.get(), br.mRenderTargetView, sr.mDimensions);
  const auto layerCount = snapshot.GetLayerCount();
  for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    const auto sourceRect
      = snapshot.GetLayerConfig(layerIndex)->mLocationOnTexture;
    const auto& destRect = destRects[layerIndex];
    const D3D11::Opacity opacity {opacities[layerIndex]};
    mSpriteBatch->Draw(
      source->GetD3D12ShaderResourceViewGPUHandle(),
      source->GetDimensions(),
      sourceRect,
      destRects[layerIndex],
      D3D12::Opacity {opacities[layerIndex]});
  }
  mSpriteBatch->End();
}

SHM::CachedReader* OpenXRD3D12Kneeboard::GetSHM() {
  return &mSHM;
}

OpenXRD3D12Kneeboard::SwapchainBufferResources::SwapchainBufferResources(
  ID3D12Device* device,
  ID3D12Resource* texture,
  D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle,
  DXGI_FORMAT renderTargetViewFormat)
  : mRenderTargetView(renderTargetViewHandle) {
  D3D12_RENDER_TARGET_VIEW_DESC desc {
    .Format = renderTargetViewFormat,
    .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {0, 0},
  };

  device->CreateRenderTargetView(texture, &desc, renderTargetViewHandle);

  winrt::check_hresult(device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocator.put())));

  winrt::check_hresult(device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    mCommandAllocator.get(),
    nullptr,
    IID_PPV_ARGS(mCommandList.put())));
}

}// namespace OpenKneeboard
