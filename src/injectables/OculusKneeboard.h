#pragma once

#include "OculusFrameHook.h"

#include "OpenKneeboard/SHM.h"

namespace OpenKneeboard {

  class OculusKneeboard : public OculusFrameHook {
  private:
    SHM::Reader mSHM;
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

  public:
    virtual ovrResult onEndFrame(
      ovrSession session,
      long long frameIndex,
      const ovrViewScaleDesc* viewScaleDesc,
      ovrLayerHeader const* const* layerPtrList,
      unsigned int layerCount,
      decltype(&ovr_EndFrame) next) override final;
  };

}// namespace OpenKneeboard
