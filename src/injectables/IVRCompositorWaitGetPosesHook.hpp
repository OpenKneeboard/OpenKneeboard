// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <functional>
#include <memory>

#include <openvr.h>

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
