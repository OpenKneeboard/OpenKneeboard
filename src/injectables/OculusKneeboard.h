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

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/VRKneeboard.h>

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
  YOrigin GetYOrigin() override;

 private:
  ovrTextureSwapChain mSwapChain = nullptr;
  ovrSession mSession = nullptr;
  Renderer* mRenderer = nullptr;
  SHM::Reader mSHM;
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
  virtual ovrTextureSwapChain CreateSwapChain(ovrSession session) = 0;
  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot,
    const VRKneeboard::RenderParameters&)
    = 0;
};

}// namespace OpenKneeboard
