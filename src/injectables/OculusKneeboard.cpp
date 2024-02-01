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
#include "OculusKneeboard.h"

#include <OpenKneeboard/Spriting.h>

#include <OpenKneeboard/dprint.h>

// clang-format off
#include <windows.h>
#include <detours.h>
// clang-format on

#include "OVRProxy.h"

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OculusKneeboard::OculusKneeboard() {
  mRenderCacheKeys.fill(~(0ui64));
}

void OculusKneeboard::InstallHook(Renderer* renderer) {
  dprintf("{} {:#018x}", __FUNCTION__, (uint64_t)this);
  mRenderer = renderer;
  mEndFrameHook.InstallHook({
    .onEndFrame = std::bind_front(&OculusKneeboard::OnOVREndFrame, this),
  });
}

OculusKneeboard::~OculusKneeboard() {
  this->UninstallHook();
  if (mRenderer) {
    delete mRenderer;
  }
}

void OculusKneeboard::UninstallHook() {
  mEndFrameHook.UninstallHook();
}

OculusKneeboard::Pose OculusKneeboard::GetHMDPose(double predictedTime) {
  auto ovr = OVRProxy::Get();
  const auto state = ovr->ovr_GetTrackingState(mSession, predictedTime, false);
  const auto& p = state.HeadPose.ThePose.Position;
  const auto& o = state.HeadPose.ThePose.Orientation;

  return {
    .mPosition = {p.x, p.y, p.z},
    .mOrientation = {o.x, o.y, o.z, o.w},
  };
}

ovrResult OculusKneeboard::OnOVREndFrame(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int origLayerCount,
  const decltype(&ovr_EndFrame)& next) {
  mSession = session;

  static bool sFirst = true;
  if (sFirst) {
    dprint(__FUNCTION__);
    sFirst = false;
  }
  auto passthrough = std::bind_front(
    next, session, frameIndex, viewScaleDesc, layerPtrList, origLayerCount);

  if (!mRenderer) {
    return passthrough();
  }

  auto shm = mRenderer->GetSHM();

  if (!(shm && *shm)) {
    return passthrough();
  }

  const auto metadata = shm->MaybeGetMetadata();
  if (!metadata.HasMetadata()) {
    return passthrough();
  }
  const auto swapchainDimensions
    = Spriting::GetBufferSize(metadata.GetLayerCount());

  if (!(mSwapchain && mSwapchainDimensions == swapchainDimensions)) {
    // It's possible for us to be injected in between a present and an
    // OVREndFrame; this means we don't have the device yet, so creation will
    // fail.
    mSwapchain = mRenderer->CreateSwapChain(session, swapchainDimensions);
    mSwapchainDimensions = swapchainDimensions;
    if (!mSwapchain) {
      traceprint("Failed to make an OVR swapchain");
      return passthrough();
    }
  }

  const auto snapshot = shm->MaybeGet();
  if (!snapshot.HasTexture()) {
    return passthrough();
  }
  const auto config = snapshot.GetConfig();

  // Copy existing layers to a mutable struct
  //
  // - drop empty layers so we have more space
  // - discard depth information if desired
  std::vector<const ovrLayerHeader*> newLayers;
  newLayers.reserve(origLayerCount + snapshot.GetLayerCount());
  std::vector<ovrLayerEyeFov> withoutDepthInformation;

  for (unsigned int i = 0; i < origLayerCount; ++i) {
    const auto layer = layerPtrList[i];

    if (!layer) {
      continue;
    }

    if (
      config.mVR.mQuirks.mOculusSDK_DiscardDepthInformation
      && layer->Type == ovrLayerType_EyeFovDepth) {
      withoutDepthInformation.push_back({});
      auto& newLayer = withoutDepthInformation.back();
      memcpy(&newLayer, layer, sizeof(ovrLayerEyeFov));
      newLayer.Header.Type = ovrLayerType_EyeFov;
      newLayers.push_back(&newLayer.Header);
      continue;
    }

    newLayers.push_back(layer);
  }

  auto ovr = OVRProxy::Get();
  const auto predictedTime
    = ovr->ovr_GetPredictedDisplayTime(session, frameIndex);

  const auto availableLayerCount = snapshot.GetLayerCount();
  std::vector<uint8_t> availableVRLayers;
  for (uint8_t i = 0; i < availableLayerCount; ++i) {
    auto config = snapshot.GetLayerConfig(i);
    if (config->mVREnabled) {
      availableVRLayers.push_back(i);
    }
  }
  const auto addedLayerCount
    = ((availableVRLayers.size() + origLayerCount) < ovrMaxLayerCount)
    ? availableVRLayers.size()
    : (ovrMaxLayerCount - origLayerCount);
  availableVRLayers.resize(addedLayerCount);

  if (addedLayerCount == 0) {
    return passthrough();
  }

  std::vector<uint64_t> cacheKeys;
  std::vector<ovrLayerQuad> addedOVRLayers;
  std::vector<SHM::LayerSprite> LayerSprite;

  cacheKeys.reserve(addedLayerCount);
  addedOVRLayers.reserve(addedLayerCount);
  LayerSprite.reserve(addedLayerCount);

  uint16_t topMost = 0;
  bool needRender = false;
  for (const auto layerIndex: availableVRLayers) {
    const auto& layer = *snapshot.GetLayerConfig(layerIndex);
    auto params = this->GetRenderParameters(
      snapshot, layer, this->GetHMDPose(predictedTime));
    cacheKeys.push_back(params.mCacheKey);

    const PixelRect destRect {
      Spriting::GetOffset(layerIndex, MaxViewCount),
      layer.mVR.mLocationOnTexture.mSize,
    };

    LayerSprite.push_back(SHM::LayerSprite {
      .mSourceRect = layer.mVR.mLocationOnTexture,
      .mDestRect = destRect,
      .mOpacity = params.mKneeboardOpacity,
    });

    if (params.mIsLookingAtKneeboard) {
      topMost = layerIndex;
    }

    auto cacheKey = mRenderCacheKeys.at(layerIndex);
    if (mRenderCacheKeys.at(layerIndex) != params.mCacheKey) {
      needRender = true;
    }

    addedOVRLayers.push_back({
      .Header = { 
        .Type = ovrLayerType_Quad,
        .Flags = {ovrLayerFlag_HighQuality},
      },
      .ColorTexture = mSwapchain,
      .Viewport = {
        .Pos = destRect.mOffset.StaticCast<int, ovrVector2i>(),
        .Size = destRect.mSize.StaticCast<int, ovrSizei>(),
      },
      .QuadPoseCenter = GetOvrPosef(params.mKneeboardPose),
      .QuadSize = {params.mKneeboardSize.x, params.mKneeboardSize.y},
    });
    newLayers.push_back(&addedOVRLayers.back().Header);
  }

  if (needRender) {
    int swapchainTextureIndex = -1;
    ovr->ovr_GetTextureSwapChainCurrentIndex(
      session, mSwapchain, &swapchainTextureIndex);
    if (swapchainTextureIndex < 0) {
      dprintf(" - invalid swap chain index ({})", swapchainTextureIndex);
      OPENKNEEBOARD_BREAK;
      return passthrough();
    }

    mRenderer->RenderLayers(
      mSwapchain,
      static_cast<uint32_t>(swapchainTextureIndex),
      snapshot,
      LayerSprite);

    auto error = ovr->ovr_CommitTextureSwapChain(session, mSwapchain);
    if (error) {
      OPENKNEEBOARD_LOG_AND_FATAL("Commit failed with {}", error);
    }

    for (size_t i = 0; i < cacheKeys.size(); ++i) {
      mRenderCacheKeys[i] = cacheKeys.at(i);
    }
  }

  if (topMost != 0) {
    std::swap(addedOVRLayers.back(), addedOVRLayers.at(topMost));
  }

  return next(
    session,
    frameIndex,
    viewScaleDesc,
    newLayers.data(),
    static_cast<unsigned int>(newLayers.size()));
}

ovrPosef OculusKneeboard::GetOvrPosef(const Pose& pose) {
  const auto& p = pose.mPosition;
  const auto& o = pose.mOrientation;
  return {
    .Orientation = {o.x, o.y, o.z, o.w},
    .Position = {p.x, p.y, p.z},
  };
}

}// namespace OpenKneeboard
