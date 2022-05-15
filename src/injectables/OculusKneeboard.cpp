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

#include <OpenKneeboard/dprint.h>

// clang-format off
#include <windows.h>
#include <detours.h>
// clang-format on

#include "OVRProxy.h"

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OculusKneeboard::OculusKneeboard() {
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

OculusKneeboard::YOrigin OculusKneeboard::GetYOrigin() {
  if (
    OVRProxy::Get()->ovr_GetTrackingOriginType(mSession)
    == ovrTrackingOrigin_EyeLevel) {
    return YOrigin::EYE_LEVEL;
  }
  return YOrigin::FLOOR_LEVEL;
}

ovrResult OculusKneeboard::OnOVREndFrame(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount,
  const decltype(&ovr_EndFrame)& next) {
  mSession = session;

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
  auto snapshot = mSHM.MaybeGet();
  if (!snapshot) {
    return passthrough();
  }
  const auto config = snapshot.GetConfig();

  if (!mSwapChain) [[unlikely]] {
    if (!mRenderer) [[unlikely]] {
      OPENKNEEBOARD_BREAK;
      return passthrough();
    }
    mSwapChain = mRenderer->CreateSwapChain(session);

    if (!mSwapChain) {
      return passthrough();
    }
  }

  auto ovr = OVRProxy::Get();
  const auto predictedTime
    = ovr->ovr_GetPredictedDisplayTime(session, frameIndex);
  const auto renderParams
    = this->GetRenderParameters(snapshot, this->GetHMDPose(predictedTime));

  static uint64_t sRenderKey = ~(0ui64);
  if (sRenderKey != renderParams.mCacheKey) {
    if (!mRenderer->Render(session, mSwapChain, snapshot, renderParams))
      [[unlikely]] {
      return passthrough();
    }
    sRenderKey = renderParams.mCacheKey;
  }

  ovrLayerQuad kneeboardLayer {
    .Header = { 
      .Type = ovrLayerType_Quad,
      .Flags = {ovrLayerFlag_HighQuality},
    },
    .ColorTexture = mSwapChain,
    .Viewport = {
      .Pos = {0, 0},
      .Size = { config.mImageWidth, config.mImageHeight },
    },
    .QuadPoseCenter = GetOvrPosef(renderParams.mKneeboardPose),
    .QuadSize = { renderParams.mKneeboardSize.x, renderParams.mKneeboardSize.y }
  };

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
  if ((config.mVR.mFlags & VRRenderConfig::Flags::DISCARD_DEPTH_INFORMATION)) {
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

ovrPosef OculusKneeboard::GetOvrPosef(const Pose& pose) {
  const auto& p = pose.mPosition;
  const auto& o = pose.mOrientation;
  return {
    .Orientation = {o.x, o.y, o.z, o.w},
    .Position = {p.x, p.y, p.z},
  };
}

}// namespace OpenKneeboard
