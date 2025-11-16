// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include "OculusKneeboard.hpp"

#include "OVRProxy.hpp"

#include <OpenKneeboard/Spriting.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <Windows.h>

#include <detours.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OculusKneeboard::OculusKneeboard() {
  mRenderCacheKeys.fill(~(0ui64));
}

void OculusKneeboard::InstallHook(Renderer* renderer) {
  dprint("{} {:#018x}", __FUNCTION__, (uint64_t)this);
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
  if (metadata.GetLayerCount() < 1) {
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
      TraceLoggingWrite(
        gTraceProvider,
        "OculusKneeboard::OnOVREndFrame()/FailedToCreateSwapChain");
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

  const auto hmdPose = this->GetHMDPose(predictedTime);
  auto vrLayers = this->GetLayers(snapshot, hmdPose);
  const auto addedLayerCount
    = ((vrLayers.size() + origLayerCount) <= ovrMaxLayerCount)
    ? vrLayers.size()
    : (ovrMaxLayerCount - origLayerCount);
  vrLayers.resize(addedLayerCount);

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
  for (size_t layerIndex = 0; layerIndex < addedLayerCount; ++layerIndex) {
    const auto [layer, params] = vrLayers.at(layerIndex);
    cacheKeys.push_back(params.mCacheKey);

    const PixelRect destRect {
      Spriting::GetOffset(layerIndex, metadata.GetLayerCount()),
      layer->mVR.mLocationOnTexture.mSize,
    };

    LayerSprite.push_back(SHM::LayerSprite {
      .mSourceRect = layer->mVR.mLocationOnTexture,
      .mDestRect = destRect,
      .mOpacity = params.mKneeboardOpacity,
    });

    if (layer->mLayerID == config.mGlobalInputLayerID) {
      topMost = layerIndex;
    }

    if (mRenderCacheKeys.at(layerIndex) != params.mCacheKey) {
      needRender = true;
    }

    addedOVRLayers.push_back({
      .Header = { 
        .Type = ovrLayerType_Quad,
        .Flags = ovrLayerFlag_HighQuality,
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
      dprint(" - invalid swap chain index ({})", swapchainTextureIndex);
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
      dprint.Warning("ovr_CommitTextureSwapChain failed with {}", error);
      return passthrough();
    }

    for (size_t i = 0; i < cacheKeys.size(); ++i) {
      mRenderCacheKeys[i] = cacheKeys.at(i);
    }
  }

  if (topMost != 0) {
    if (topMost < addedOVRLayers.size()) [[likely]] {
      std::swap(addedOVRLayers.back(), addedOVRLayers.at(topMost));
    } else {
      dprint(
        "topMost layer has index {}, but count is {}",
        topMost,
        addedOVRLayers.size());
      OPENKNEEBOARD_BREAK;
    }
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

OculusKneeboard::Renderer::~Renderer() = default;

}// namespace OpenKneeboard
