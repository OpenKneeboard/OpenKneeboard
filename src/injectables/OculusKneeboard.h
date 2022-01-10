#pragma once

#include <OpenKneeboard/SHM.h>

#include "InjectedKneeboard.h"
#include "OculusFrameHook.h"

namespace OpenKneeboard {

class OculusKneeboard : public OculusFrameHook, public InjectedKneeboard {
 private:
  SHM::Reader mSHM;
  bool mZoomed = false;

 protected:
  OculusKneeboard();
  virtual ~OculusKneeboard();

  virtual ovrTextureSwapChain GetSwapChain(
    ovrSession session,
    const SHM::Header& config)
    = 0;
  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot)
    = 0;

  virtual void Unhook() override;

 public:
  virtual ovrResult OnEndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    const decltype(&ovr_EndFrame)& next) override final;
};

}// namespace OpenKneeboard
