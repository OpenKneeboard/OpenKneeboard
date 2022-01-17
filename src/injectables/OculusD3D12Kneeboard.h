#pragma once

#include "ID3D12CommandQueueExecuteCommandListsHook.h"
#include "OculusKneeboard.h"

namespace OpenKneeboard {

class OculusD3D12Kneeboard final
  : private OculusKneeboard::Renderer {
 public:
  OculusD3D12Kneeboard();
  virtual ~OculusD3D12Kneeboard();

  void UninstallHook();

 protected:
  virtual ovrTextureSwapChain GetSwapChain(
    ovrSession session,
    const SHM::Config& config) override;

  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot) override;

  void OnID3D12CommandQueue_ExecuteCommandLists(
    ID3D12CommandQueue* this_,
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists,
    const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next);

 private:
  ID3D12CommandQueueExecuteCommandListsHook mExecuteCommandListsHook;
};
}// namespace OpenKneeboard
