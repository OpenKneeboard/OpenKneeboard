// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <functional>
#include <memory>

#include <OVR_CAPI.h>

namespace OpenKneeboard {

/** Hook for `ovrEndFrame`/`ovrSubmitFrame`/`ovrSubmitFrame2`.
 *
 * These all have the same signature and serve a similar purpose. Each app
 * should only use one of these - which one depends on which version of the
 * SDK they're using, and if they're using the current best practices.
 *
 * The end frame callback will be invoked for all of them, though the `next`
 * parameter might be pointing at `ovrSubmitFrame` or `ovrSubmitFrame2` instead
 * of `ovrEndFrame`.
 */
class OculusEndFrameHook final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  OculusEndFrameHook();
  ~OculusEndFrameHook();

  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<ovrResult(
      ovrSession session,
      long long frameIndex,
      const ovrViewScaleDesc* viewScaleDesc,
      ovrLayerHeader const* const* layerPtrList,
      unsigned int layerCount,
      const decltype(&ovr_EndFrame)& next)>
      onEndFrame;
  };

  void InstallHook(const Callbacks&);
  void UninstallHook();
};

}// namespace OpenKneeboard
