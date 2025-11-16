// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "OculusEndFrameHook.hpp"

#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/VRKneeboard.hpp>

#include <OpenKneeboard/config.hpp>

#include <span>

namespace OpenKneeboard {

class OculusKneeboard final : private VRKneeboard {
 public:
  class Renderer;
  OculusKneeboard();
  virtual ~OculusKneeboard();

  void InstallHook(Renderer* renderer);
  void UninstallHook();

 protected:
  Pose GetHMDPose(double predictedTime);

 private:
  ovrTextureSwapChain mSwapchain {nullptr};
  PixelSize mSwapchainDimensions;
  std::array<uint64_t, MaxViewCount> mRenderCacheKeys;
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
  virtual ~Renderer();

  virtual SHM::CachedReader* GetSHM() = 0;

  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    const PixelSize&)
    = 0;

  virtual void RenderLayers(
    ovrTextureSwapChain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    const std::span<SHM::LayerSprite>& layers)
    = 0;
};

}// namespace OpenKneeboard
