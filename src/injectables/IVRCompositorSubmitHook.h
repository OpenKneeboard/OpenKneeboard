#pragma once

#include <openvr.h>

#include <memory>

namespace OpenKneeboard {

/** Hook for OpenVR's `vr::VRCompositor()->Submit()`.
 *
 * This hook is used by `AutoDetectKneeboard` to see whether or not an
 * application is actively using OpenVR, rather than just linking against it.
 *
 * If OpenVR usage is detected, `AutoDetectKneeboard` will not load any
 * kneeboard; as OpenVR directly supports separate overlay applications, we use
 * that API in the main OpenKneeboard application instead of in the game
 * process.
 */
class IVRCompositorSubmitHook {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  IVRCompositorSubmitHook();
  ~IVRCompositorSubmitHook();
  void Unhook();

 protected:
  bool IsHookInstalled() const;

  virtual vr::EVRCompositorError OnIVRCompositor_Submit(
    vr::EVREye eEye,
    const vr::Texture_t* pTexture,
    const vr::VRTextureBounds_t* pBounds,
    vr::EVRSubmitFlags nSubmitFlags,
    vr::IVRCompositor* compositor,
    const decltype(&vr::IVRCompositor::Submit)& next)
    = 0;
};

}// namespace OpenKneeboard
