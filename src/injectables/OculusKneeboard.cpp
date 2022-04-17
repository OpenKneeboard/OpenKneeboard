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

#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/RayIntersectsRect.h>
#include <OpenKneeboard/dprint.h>

// clang-format off
#include <windows.h>
#include <detours.h>
// clang-format on

#include "OVRProxy.h"

using namespace OpenKneeboard;
using namespace DirectX::SimpleMath;

static bool poseIntersectsRect(
  const ovrPosef& pose,
  const Vector3& rectCenter,
  const Quaternion& rectQuat,
  const Vector2& rectSize) {
  const Quaternion rayQuat(
    pose.Orientation.x,
    pose.Orientation.y,
    pose.Orientation.z,
    pose.Orientation.w);
  const Vector3 rayOrigin(pose.Position.x, pose.Position.y, pose.Position.z);

  return RayIntersectsRect(rayOrigin, rayQuat, rectCenter, rectQuat, rectSize);
}

namespace OpenKneeboard {

struct OculusKneeboard::Impl {
  Impl(Renderer*);

  Renderer* mRenderer = nullptr;
  bool mZoomed = false;

  SHM::Reader mSHM;
  OculusEndFrameHook mEndFrameHook;
  uint32_t mLastSequenceNumber;

  ovrResult OnOVREndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    const decltype(&ovr_EndFrame)& next);

  ovrTextureSwapChain GetSwapChain(
    ovrSession session,
    const SHM::Config& config);

 private:
  SHM::Config mLastConfig;
  ovrTextureSwapChain mSwapChain = nullptr;
};

OculusKneeboard::OculusKneeboard() {
}

void OculusKneeboard::InstallHook(Renderer* renderer) {
  if (p) {
    throw std::logic_error("Can't install OculusKneeboard twice");
  }
  p = std::make_unique<Impl>(renderer);
  dprintf("{} {:#018x}", __FUNCTION__, (uint64_t)this);
}

OculusKneeboard::~OculusKneeboard() {
  this->UninstallHook();
}

void OculusKneeboard::UninstallHook() {
  if (p) {
    p->mEndFrameHook.UninstallHook();
  }
}

OculusKneeboard::Impl::Impl(Renderer* renderer) : mRenderer(renderer) {
  mEndFrameHook.InstallHook({
    .onEndFrame = std::bind_front(&Impl::OnOVREndFrame, this),
  });
}

ovrTextureSwapChain OculusKneeboard::Impl::GetSwapChain(
  ovrSession session,
  const SHM::Config& config) {
  auto ovr = OVRProxy::Get();
  if (mSwapChain) {
    if (
      config.imageWidth == mLastConfig.imageWidth
      && config.imageHeight == mLastConfig.imageHeight) {
      return mSwapChain;
    }

    ovr->ovr_DestroyTextureSwapChain(session, mSwapChain);
  }
  mSwapChain = mRenderer->CreateSwapChain(session, config);
  return mSwapChain;
}

ovrResult OculusKneeboard::Impl::OnOVREndFrame(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount,
  const decltype(&ovr_EndFrame)& next) {
  static bool sFirst = true;
  if (sFirst) {
    dprint(__FUNCTION__);
    sFirst = false;
  }
  auto passthrough = std::bind_front(
    next, session, frameIndex, viewScaleDesc, layerPtrList, layerCount);

  if (!(mSHM && mRenderer)) [[unlikely]] {
    return passthrough();
  }

  if (mLastSequenceNumber != mSHM.GetSequenceNumber() || !mSwapChain)
    [[unlikely]] {
    auto snapshot = mSHM.MaybeGet();
    if (!snapshot) {
      return passthrough();
    }
    const auto config = snapshot.GetConfig();

    mSwapChain = this->GetSwapChain(session, config);

    if (!mSwapChain) {
      return passthrough();
    }

    if (!mRenderer->Render(session, mSwapChain, snapshot)) {
      return passthrough();
    }
    mLastSequenceNumber = snapshot.GetSequenceNumber();
    mLastConfig = config;
  }

  auto ovr = OVRProxy::Get();
  auto config = mLastConfig;

  ovrLayerQuad kneeboardLayer = {};
  kneeboardLayer.Header.Type = ovrLayerType_Quad;
  // TODO: set ovrLayerFlag_TextureOriginAtBottomLeft for OpenGL?
  kneeboardLayer.Header.Flags = {ovrLayerFlag_HighQuality};
  kneeboardLayer.ColorTexture = mSwapChain;

  const auto& vr = config.vr;
  Vector3 position(vr.mX, vr.mFloorY, vr.mZ);
  if ((config.vr.mFlags & VRRenderConfig::Flags::HEADLOCKED)) {
    kneeboardLayer.Header.Flags |= ovrLayerFlag_HeadLocked;
  } else if (
    ovr->ovr_GetTrackingOriginType(session) == ovrTrackingOrigin_EyeLevel) {
    position.y = vr.mEyeY;
  }
  kneeboardLayer.QuadPoseCenter.Position = {position.x, position.y, position.z};

  // clang-format off
  auto orientation =
    Quaternion::CreateFromAxisAngle(Vector3::UnitX, vr.mRX)
    * Quaternion::CreateFromAxisAngle(Vector3::UnitY, vr.mRY)
    * Quaternion::CreateFromAxisAngle(Vector3::UnitZ, vr.mRZ);
  // clang-format on

  kneeboardLayer.QuadPoseCenter.Orientation
    = {orientation.x, orientation.y, orientation.z, orientation.w};

  kneeboardLayer.Viewport.Pos = {.x = 0, .y = 0};
  kneeboardLayer.Viewport.Size
    = {.w = config.imageWidth, .h = config.imageHeight};

  const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
  const auto virtualHeight = vr.mHeight;
  const auto virtualWidth = aspectRatio * vr.mHeight;

  const ovrVector2f normalSize(virtualWidth, virtualHeight);
  const ovrVector2f zoomedSize(
    virtualWidth * vr.mZoomScale, virtualHeight * vr.mZoomScale);

  if (vr.mFlags & VRRenderConfig::Flags::FORCE_ZOOM) {
    mZoomed = true;
  } else if (!(vr.mFlags & VRRenderConfig::Flags::GAZE_ZOOM)) {
    mZoomed = false;
  } else if (
    vr.mZoomScale < 1.1 || vr.mGazeTargetHorizontalScale < 0.1
    || vr.mGazeTargetVerticalScale < 0.1) {
    mZoomed = false;
  } else {
    const auto predictedTime
      = ovr->ovr_GetPredictedDisplayTime(session, frameIndex);
    const auto currentSize = mZoomed ? zoomedSize : normalSize;
    const auto state = ovr->ovr_GetTrackingState(session, predictedTime, false);

    mZoomed = poseIntersectsRect(
      state.HeadPose.ThePose,
      position,
      orientation,
      {
        currentSize.x * vr.mGazeTargetHorizontalScale,
        currentSize.y * vr.mGazeTargetVerticalScale,
      });
  }

  kneeboardLayer.QuadSize = mZoomed ? zoomedSize : normalSize;

  std::vector<const ovrLayerHeader*> newLayers;
  if (layerCount == 0) {
    newLayers.push_back(&kneeboardLayer.Header);
  } else if (layerCount < ovrMaxLayerCount) {
    newLayers = {&layerPtrList[0], &layerPtrList[layerCount]};
    newLayers.push_back(&kneeboardLayer.Header);
  } else {
    for (unsigned int i = 0; i < layerCount; ++i) {
      if (layerPtrList[i]) {
        newLayers.push_back(layerPtrList[i]);
      }
    }

    if (newLayers.size() < ovrMaxLayerCount) {
      newLayers.push_back(&kneeboardLayer.Header);
    } else {
      dprintf("Already at ovrMaxLayerCount without adding our layer");
    }
  }

  std::vector<ovrLayerEyeFov> withoutDepthInformation;
  if ((config.vr.mFlags & VRRenderConfig::Flags::DISCARD_DEPTH_INFORMATION)) {
    for (auto i = 0; i < newLayers.size(); ++i) {
      auto layer = newLayers.at(i);
      if (layer->Type != ovrLayerType_EyeFovDepth) {
        continue;
      }

      withoutDepthInformation.push_back({});
      auto& newLayer
        = withoutDepthInformation[withoutDepthInformation.size() - 1];
      memcpy(&newLayer, layer, sizeof(ovrLayerEyeFov));
      newLayer.Header.Type = ovrLayerType_EyeFov;

      newLayers[i] = &newLayer.Header;
    }
  }

  return next(
    session,
    frameIndex,
    viewScaleDesc,
    newLayers.data(),
    static_cast<unsigned int>(newLayers.size()));
}

}// namespace OpenKneeboard
