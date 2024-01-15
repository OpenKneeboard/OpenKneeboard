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
#include <shims/winrt/base.h>
// clang-format on

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/VRKneeboard.h>
#include <OpenKneeboard/config.h>

#include "OculusEndFrameHook.h"

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
  std::array<ovrTextureSwapChain, MaxLayers> mSwapChains;
  std::array<uint64_t, MaxLayers> mRenderCacheKeys;
  SHM::CachedReader mSHM;
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
  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    uint8_t layerIndex)
    = 0;
  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot,
    uint8_t layerIndex,
    const VRKneeboard::RenderParameters&)
    = 0;

  virtual SHM::ConsumerKind GetConsumerKind() const = 0;
  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() = 0;
};

}// namespace OpenKneeboard
