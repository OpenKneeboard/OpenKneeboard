#pragma once

#include <OpenKneeboard/SHM.h>

#include "OculusEndFrameHook.h"

namespace OpenKneeboard {

class OculusKneeboard : public OculusEndFrameHook {
 protected:
  OculusKneeboard();
  virtual ~OculusKneeboard();

  virtual ovrTextureSwapChain GetSwapChain(
    ovrSession session,
    const SHM::Config& config)
    = 0;
  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot)
    = 0;

  virtual void UninstallHook();

  virtual void OnOVREndFrameHookInstalled() override;
  virtual ovrResult OnOVREndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    const decltype(&ovr_EndFrame)& next) override final;
 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
