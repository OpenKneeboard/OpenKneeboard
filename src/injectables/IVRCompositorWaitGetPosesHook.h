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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#pragma once

#include <openvr.h>

#include <functional>
#include <memory>

namespace OpenKneeboard {

/** Hook for OpenVR's `vr::VRCompositor()->Submit()`.
 *
 * This hook is used by `InjectionBootstrapper` to see whether or not an
 * application is actively using OpenVR, rather than just linking against it.
 *
 * If OpenVR usage is detected, `InjectionBootstrapper` will not load any
 * kneeboard; as OpenVR directly supports separate overlay applications, we use
 * that API in the main OpenKneeboard application instead of in the game
 * process.
 */
class IVRCompositorWaitGetPosesHook final {
 public:
  IVRCompositorWaitGetPosesHook();
  ~IVRCompositorWaitGetPosesHook();

  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<vr::EVRCompositorError(
      vr::IVRCompositor* this_,
      vr::TrackedDevicePose_t* pRenderPoseArray,
      uint32_t unRenderPoseArrayCount,
      vr::TrackedDevicePose_t* pGamePoseArray,
      uint32_t unGamePoseArrayCount,
      const decltype(&vr::IVRCompositor::WaitGetPoses)& next)>
      onWaitGetPoses;
  };

  void InstallHook(const Callbacks& cb);
  void UninstallHook();

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
