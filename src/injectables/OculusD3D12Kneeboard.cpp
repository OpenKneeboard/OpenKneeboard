#include "OculusD3D12Kneeboard.h"

namespace OpenKneeboard {

struct OculusD3D12Kneeboard::Impl {};

OculusD3D12Kneeboard::OculusD3D12Kneeboard() : p(std::make_unique<Impl>()) {
  ID3D12CommandQueueExecuteCommandListsHook::InitWithVTable();
}

OculusD3D12Kneeboard::~OculusD3D12Kneeboard() {
  UninstallHook();
}

void OculusD3D12Kneeboard::UninstallHook() {
  ID3D12CommandQueueExecuteCommandListsHook::UninstallHook();
}

ovrTextureSwapChain OculusD3D12Kneeboard::GetSwapChain(
  ovrSession session,
  const SHM::Config& config) {
  return {};
}

bool OculusD3D12Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot) {
  return false;
}

void OculusD3D12Kneeboard::OnID3D12CommandQueue_ExecuteCommandLists(
  ID3D12CommandQueue* this_,
  UINT NumCommandLists,
  ID3D12CommandList* const* ppCommandLists,
  const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next) {
  return std::invoke(next, this_, NumCommandLists, ppCommandLists);
}

}// namespace OpenKneeboard
