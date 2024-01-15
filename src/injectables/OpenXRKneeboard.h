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

// clang-format off
#include <Windows.h>
#include <d3d11.h>
// clang-format on

#include <OpenKneeboard/VRKneeboard.h>

#include <OpenKneeboard/config.h>

#include <openxr/openxr.h>

#include <format>

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
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&);
  virtual ~OpenXRKneeboard();

  XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo);

  struct LayerRenderInfo {
    uint8_t mLayerIndex;
    PixelRect mSourceRect;
    PixelRect mDestRect;
    VRKneeboard::RenderParameters mVR;
  };

 protected:
  virtual bool ConfigurationsAreCompatible(
    const VRRenderConfig& initial,
    const VRRenderConfig& current) const
    = 0;
  virtual XrSwapchain CreateSwapchain(
    XrSession,
    const PixelSize&,
    const VRRenderConfig::Quirks& quirks)
    = 0;
  // Release any buffers, views, caches etc, but do not destroy the swap chain
  virtual void ReleaseSwapchainResources(XrSwapchain) = 0;

  virtual bool RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    uint8_t layerCount,
    LayerRenderInfo* layers)
    = 0;
  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() = 0;

  // For quirks
  bool IsVarjoRuntime() const;

  OpenXRNext* GetOpenXR();

 private:
  std::shared_ptr<OpenXRNext> mOpenXR;
  SHM::CachedReader mSHM;

  XrSwapchain mSwapchain {};
  std::array<uint64_t, MaxLayers> mRenderCacheKeys;

  XrSpace mLocalSpace = nullptr;
  XrSpace mViewSpace = nullptr;

  // For quirks
  bool mIsVarjoRuntime = false;
  VRRenderConfig mInitialConfig;

  Pose GetHMDPose(XrTime displayTime);
  static XrPosef GetXrPosef(const Pose& pose);
};

}// namespace OpenKneeboard
