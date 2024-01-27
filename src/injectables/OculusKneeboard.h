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
#pragma once

#include "OculusEndFrameHook.h"

#include <OpenKneeboard/Pixels.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/VRKneeboard.h>

#include <OpenKneeboard/config.h>

namespace OpenKneeboard {

class OculusKneeboard final : private VRKneeboard {
 public:
  class Renderer;
  OculusKneeboard();
  ~OculusKneeboard();

  void InstallHook(Renderer* renderer);
  void UninstallHook();

 protected:
  Pose GetHMDPose(double predictedTime);

 private:
  ovrTextureSwapChain mSwapchain {nullptr};
  std::array<uint64_t, MaxLayers> mRenderCacheKeys;
  ovrSession mSession = nullptr;
  Renderer* mRenderer = nullptr;
  OculusEndFrameHook mEndFrameHook;

  ovrResult OnOVREndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    const decltype(&ovr_EndFrame)& next);

  static ovrPosef GetOvrPosef(const Pose&);
};

class OculusKneeboard::Renderer {
 public:
  virtual SHM::CachedReader* GetSHM() = 0;

  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    const PixelSize&)
    = 0;

  virtual void RenderLayers(
    ovrTextureSwapChain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    const PixelRect* const destRects,
    const float* const opacities)
    = 0;
};

}// namespace OpenKneeboard
