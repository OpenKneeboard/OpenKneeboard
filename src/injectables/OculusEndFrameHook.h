#pragma once

#include <OVR_CAPI.H>

#include <memory>

namespace OpenKneeboard {

/** Hook for `ovrEndFrame`/`ovrSubmitFrame`/`ovrSubmitFrame2`.
 *
 * These all have the same signature and serve a similar purpose. Each app
 * should only use one of these - which one depends on which version of the
 * SDK they're using, and if they're using the current best practices.
 * 
 * `OnOVREndFrame()` will be invoked for all of them, though the `next`
 * parameter might be pointing at `ovrSubmitFrame` or `ovrSubmitFrame2` instead
 * of `ovrEndFrame`.
 */
class OculusEndFrameHook {
 private:
  bool mHooked;

 public:
  OculusEndFrameHook();
  virtual ~OculusEndFrameHook();

  void Unhook();

  virtual ovrResult OnOVREndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    const decltype(&ovr_EndFrame)& next)
    = 0;
};

}// namespace OpenKneeboard
