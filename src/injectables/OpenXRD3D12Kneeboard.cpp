// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenXRD3D12Kneeboard.hpp"

#include "OpenXRD3D11Kneeboard.hpp"
#include "OpenXRNext.hpp"

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <d3d11.h>
#include <d3d12.h>

#include <string>

#include <d3d11on12.h>

#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {

OpenXRD3D12Kneeboard::OpenXRD3D12Kneeboard(
  XrInstance instance,
  XrSystemId systemID,
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D12KHR& binding)
  : OpenXRKneeboard(instance, systemID, session, runtimeID, next) {
  dprint("{}", __FUNCTION__);

  mGraphicsMemory = std::make_unique<DirectX::GraphicsMemory>(binding.device);

  mDevice.copy_from(binding.device);
  mCommandQueue.copy_from(binding.queue);
  mRenderer = std::make_unique<D3D12::Renderer>(
    mDevice.get(), mCommandQueue.get(), DXGI_FORMAT_B8G8R8A8_UNORM);

  mSHM = std::make_unique<SHM::D3D12::Reader>(
    SHM::ConsumerKind::OpenXR_D3D12, mDevice.get());
}

OpenXRD3D12Kneeboard::~OpenXRD3D12Kneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRD3D12Kneeboard()");
}

XrSwapchain OpenXRD3D12Kneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size) {
  dprint("{}", __FUNCTION__);
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D12Kneeboard::CreateSwapchain");

  auto oxr = this->GetOpenXR();

  auto formats = OpenXRD3D11Kneeboard::GetDXGIFormats(oxr, session);
  dprint(
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
    dprint("Failed to create swapchain: {}", nextResult);
    return nullptr;
  }

  uint32_t imageCount = 0;
  nextResult
    = oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || nextResult != XR_SUCCESS) {
    dprint("No images in swapchain: {}", nextResult);
    return nullptr;
  }

  dprint("{} images in swapchain", imageCount);

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
    dprint("Failed to enumerate images in swapchain: {}", nextResult);
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

  static std::atomic<uint64_t> sSwapChainCount;
  const auto thisSwapchain = sSwapChainCount++;

  auto& sr = mSwapchainResources.at(swapchain);
  for (const auto& image: images) {
    const auto imageIndex = sr.mBufferResources.size();
    sr.mBufferResources.push_back({
      mDevice.get(),
      image.texture,
      sr.mRenderTargetViewHeap.GetCpuHandle(imageIndex),
      formats.mRenderTargetViewFormat,
    });
    image.texture->SetName(
      std::format(
        L"OpenKneeboard D3D12 OpenXR swapchain #{} subimage #{}",
        thisSwapchain,
        imageIndex)
        .c_str());
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
  SHM::Frame frame,
  const std::span<SHM::LayerSprite>& layers) {
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D12Kneeboard::RenderLayers()");

  mRenderer->RenderLayers(
    mSwapchainResources.at(swapchain),
    swapchainTextureIndex,
    mSHM->Map(std::move(frame)),
    layers,
    RenderMode::ClearAndRender);
  mGraphicsMemory->Commit(mCommandQueue.get());
}

SHM::Reader& OpenXRD3D12Kneeboard::GetSHM() {
  return *mSHM;
}

}// namespace OpenKneeboard
