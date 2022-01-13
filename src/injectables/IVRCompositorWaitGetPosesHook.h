#pragma once

#include <openvr.h>

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
class IVRCompositorWaitGetPosesHook {
 public:
  struct Impl;
  ~IVRCompositorWaitGetPosesHook();
  void UninstallHook();

 protected:
  IVRCompositorWaitGetPosesHook();
  /// Call this from your constructor
  void InitWithVTable();

  virtual vr::EVRCompositorError OnIVRCompositor_WaitGetPoses(
    vr::IVRCompositor* this_,
    vr::TrackedDevicePose_t* pRenderPoseArray,
    uint32_t unRenderPoseArrayCount,
    vr::TrackedDevicePose_t* pGamePoseArray,
    uint32_t unGamePoseArrayCount,
    const decltype(&vr::IVRCompositor::WaitGetPoses)& next)
    = 0;

 private:
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
