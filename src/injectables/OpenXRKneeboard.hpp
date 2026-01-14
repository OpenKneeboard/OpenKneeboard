// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#pragma once

#include <OpenKneeboard/VRKneeboard.hpp>

#include <OpenKneeboard/config.hpp>

#include <shims/openxr/openxr.h>

#include <Windows.h>
#include <d3d11.h>

#include <format>
#include <source_location>
#include <span>

template <class CharT>
struct std::formatter<XrResult, CharT> : std::formatter<int, CharT> {};

namespace OpenKneeboard {

struct OpenXRRuntimeID {
  XrVersion mVersion;
  char mName[XR_MAX_RUNTIME_NAME_SIZE];
};

class OpenXRNext;

class OpenXRKneeboard : public VRKneeboard {
 public:
  OpenXRKneeboard() = delete;
  OpenXRKneeboard(
    XrInstance,
    XrSystemId,
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&);
  virtual ~OpenXRKneeboard();

  XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo);

 protected:
  virtual XrSwapchain CreateSwapchain(XrSession, const PixelSize&) = 0;
  // Release any buffers, views, caches etc, but do not destroy the swap chain
  virtual void ReleaseSwapchainResources(XrSwapchain) = 0;

  virtual void RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    SHM::Frame frame,
    const std::span<SHM::LayerSprite>& layers) = 0;
  virtual SHM::Reader& GetSHM() = 0;

  OpenXRNext* GetOpenXR();

 private:
  std::shared_ptr<OpenXRNext> mOpenXR;
  XrInstance mXRInstance;

  uint32_t mMaxLayerCount {};

  XrSwapchain mSwapchain {};
  PixelSize mSwapchainDimensions;
  std::array<uint64_t, MaxViewCount> mRenderCacheKeys;

  XrSpace mLocalSpace = nullptr;
  XrSpace mViewSpace = nullptr;

  bool mIsVarjoRuntime {false};

  Pose GetHMDPose(XrTime displayTime);
  static XrPosef GetXrPosef(const Pose& pose);
};
}// namespace OpenKneeboard
