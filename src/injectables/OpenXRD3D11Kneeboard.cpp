// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenXRD3D11Kneeboard.hpp"

#include "OpenXRNext.hpp"

#include <OpenKneeboard/D3D11.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <d3d11.h>

#include <directxtk/SpriteBatch.h>

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrInstance instance,
  XrSystemId systemID,
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D11KHR& binding)
  : OpenXRKneeboard(instance, systemID, session, runtimeID, next) {
  dprint("{}", __FUNCTION__);
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D11Kneeboard()");

  mDevice.copy_from(binding.device);
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  mDevice->GetImmediateContext(ctx.put());
  mImmediateContext = ctx.as<ID3D11DeviceContext1>();

  mRenderer = std::make_unique<D3D11::Renderer>(mDevice.get());

  mSHM = std::make_unique<SHM::D3D11::Reader>(
    SHM::ConsumerKind::OpenXR_D3D11, mDevice.get());
}

OpenXRD3D11Kneeboard::~OpenXRD3D11Kneeboard() {
  OPENKNEEBOARD_TraceLoggingScope("~OpenXRD3D11Kneeboard()");
}

OpenXRD3D11Kneeboard::DXGIFormats OpenXRD3D11Kneeboard::GetDXGIFormats(
  OpenXRNext* oxr,
  XrSession session) {
  uint32_t formatCount {0};
  if (XR_FAILED(
        oxr->xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr))) {
    dprint("Failed to get swapchain format count");
    return {};
  }
  std::vector<int64_t> formats;
  formats.resize(formatCount);
  if (
    XR_FAILED(oxr->xrEnumerateSwapchainFormats(
      session, formatCount, &formatCount, formats.data()))
    || formatCount == 0) {
    dprint("Failed to enumerate swapchain formats");
    return {};
  }
  for (const auto it: formats) {
    dprint("Runtime supports swapchain format: {}", it);
  }
  // If this changes, we probably want to change the preference list below
  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  std::vector<DXGIFormats> preferredFormats {
    {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM},
    {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM},
  };
  for (const auto preferred: preferredFormats) {
    auto it = std::ranges::find(formats, preferred.mTextureFormat);
    if (it != formats.end()) {
      return preferred;
    }
  }

  auto format = static_cast<DXGI_FORMAT>(formats.front());
  return {format, format};
}

XrSwapchain OpenXRD3D11Kneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size) {
  dprint("{}", __FUNCTION__);
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D11Kneeboard::CreateSwapchain()");

  auto oxr = this->GetOpenXR();

  auto formats = GetDXGIFormats(oxr, session);
  dprint(
    "Creating swapchain with format {}",
    static_cast<int>(formats.mTextureFormat));

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
  if (XR_FAILED(nextResult)) {
    dprint("Failed to create swapchain: {}", nextResult);
    return nullptr;
  }

  uint32_t imageCount = 0;
  nextResult
    = oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || XR_FAILED(nextResult)) {
    dprint("No images in swapchain: {}", nextResult);
    return nullptr;
  }

  dprint("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageD3D11KHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageD3D11KHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,
    });
  nextResult = oxr->xrEnumerateSwapchainImages(
    swapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (XR_FAILED(nextResult)) {
    dprint("Failed to enumerate images in swapchain: {}", nextResult);
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
    dprint("Swap chain is not a D3D11 swapchain");
    OPENKNEEBOARD_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  std::vector<SwapchainBufferResources> buffers;
  buffers.reserve(imageCount);
  for (size_t i = 0; i < imageCount; ++i) {
    auto& image = images.at(i);
#ifdef DEBUG
    if (image.type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
      OPENKNEEBOARD_BREAK;
    }
#endif
    buffers.push_back(
      {mDevice.get(), image.texture, formats.mRenderTargetViewFormat});
  }

  mSwapchainResources[swapchain] = {
    .mDimensions = size,
    .mBufferResources = std::move(buffers),
  };

  return swapchain;
}

void OpenXRD3D11Kneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (mSwapchainResources.contains(swapchain)) {
    mSwapchainResources.erase(swapchain);
  }
}

void OpenXRD3D11Kneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  SHM::Frame frame,
  const std::span<SHM::LayerSprite>& layers) {
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D11Kneeboard::RenderLayers()");

  D3D11::ScopedDeviceContextStateChange savedState(
    mImmediateContext, &mRenderState);

  mRenderer->RenderLayers(
    mSwapchainResources.at(swapchain),
    swapchainTextureIndex,
    mSHM->Map(std::move(frame)),
    layers,
    RenderMode::ClearAndRender);
}

SHM::Reader& OpenXRD3D11Kneeboard::GetSHM() {
  return *mSHM;
}

}// namespace OpenKneeboard
