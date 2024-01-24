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

#include "OpenXRD3D11Kneeboard.h"

#include "OpenXRNext.h"

#include <OpenKneeboard/D3D11.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <directxtk/SpriteBatch.h>

#include <d3d11.h>

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D11KHR& binding)
  : OpenXRKneeboard(session, runtimeID, next) {
  dprintf("{}", __FUNCTION__);
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D11Kneeboard()");

  mDevice.copy_from(binding.device);
  mDevice->GetImmediateContext(mImmediateContext.put());

  mSpriteBatch = std::make_unique<D3D11::SpriteBatch>(binding.device);
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
    dprintf("Runtime supports swapchain format: {}", it);
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
  const PixelSize& size,
  const VRRenderConfig::Quirks&) {
  dprintf("{}", __FUNCTION__);

  auto oxr = this->GetOpenXR();

  auto formats = GetDXGIFormats(oxr, session);
  dprintf(
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
    dprintf("Failed to create swapchain: {}", nextResult);
    return nullptr;
  }

  uint32_t imageCount = 0;
  nextResult
    = oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || XR_FAILED(nextResult)) {
    dprintf("No images in swapchain: {}", nextResult);
    return nullptr;
  }

  dprintf("{} images in swapchain", imageCount);

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
    dprintf("Failed to enumerate images in swapchain: {}", nextResult);
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

OpenXRD3D11Kneeboard::SwapchainBufferResources::SwapchainBufferResources(
  ID3D11Device* device,
  ID3D11Texture2D* texture,
  DXGI_FORMAT renderTargetViewFormat) {
  mTexture = texture;

  D3D11_TEXTURE2D_DESC textureDesc;
  texture->GetDesc(&textureDesc);

  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc {
    .Format = renderTargetViewFormat,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {0},
  };
  winrt::check_hresult(
    device->CreateRenderTargetView(texture, &rtvDesc, mRenderTargetView.put()));
}

void OpenXRD3D11Kneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (mSwapchainResources.contains(swapchain)) {
    mSwapchainResources.erase(swapchain);
  }
}

void OpenXRD3D11Kneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  const PixelRect* const destRects,
  const float* const opacities) {
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D11Kneeboard::RenderLayers()");
  D3D11::SavedState savedState(mImmediateContext);

  auto source
    = snapshot.GetTexture<SHM::D3D11::Texture>()->GetD3D11ShaderResourceView();

  const auto& sr = mSwapchainResources.at(swapchain);
  const auto& br = sr.mBufferResources.at(swapchainTextureIndex);

  mSpriteBatch->Begin(br.mRenderTargetView.get(), sr.mDimensions);
  const auto layerCount = snapshot.GetLayerCount();
  for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    const auto sourceRect
      = snapshot.GetLayerConfig(layerIndex)->mLocationOnTexture;
    const auto& destRect = destRects[layerIndex];
    const D3D11::Opacity opacity {opacities[layerIndex]};
    mSpriteBatch->Draw(source, sourceRect, destRect, opacity);
  }
  mSpriteBatch->End();
}

SHM::CachedReader* OpenXRD3D11Kneeboard::GetSHM() {
  return &mSHM;
}

}// namespace OpenKneeboard
