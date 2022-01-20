#pragma once

#include <OpenKneeboard/SHM.h>

#include "OculusEndFrameHook.h"

namespace OpenKneeboard {

class OculusKneeboard final {
 public :
  class Renderer;
  OculusKneeboard(Renderer* renderer);
  ~OculusKneeboard();

  void UninstallHook();

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

class OculusKneeboard::Renderer {
 public:
  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    const SHM::Config& config)
    = 0;
  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot)
    = 0;
};

}// namespace OpenKneeboard
