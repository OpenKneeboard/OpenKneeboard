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
    const SHM::Snapshot& snapshot,
    const std::span<SHM::LayerSprite>& layers)
    = 0;
  virtual SHM::CachedReader* GetSHM() = 0;

  OpenXRNext* GetOpenXR();

 private:
  std::shared_ptr<OpenXRNext> mOpenXR;
  XrInstance mXRInstance;
  uint64_t mSessionID {};

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
