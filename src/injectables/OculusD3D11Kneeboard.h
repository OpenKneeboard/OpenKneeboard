#pragma once

#include "OculusKneeboard.h"

namespace OpenKneeboard {

class OculusD3D11Kneeboard final : public OculusKneeboard {
 public:
  OculusD3D11Kneeboard();
  virtual ~OculusD3D11Kneeboard();

 protected:
  virtual ovrTextureSwapChain GetSwapChain(
    ovrSession session,
    const SHM::Header& config) override final;
  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot) override final;
  private:
   class Impl;
   std::unique_ptr<Impl> p;
};
}// namespace OpenKneeboard
