#pragma once

#include <d3d12.h>

#include <memory>

namespace OpenKneeboard {

class ID3D12CommandQueueExecuteCommandListsHook {
 private:
  struct Impl;

 public:
  ~ID3D12CommandQueueExecuteCommandListsHook();

  void UninstallHook();

 protected:
  ID3D12CommandQueueExecuteCommandListsHook();
  void InitWithVTable();

  virtual void OnID3D12CommandQueue_ExecuteCommandLists(
    ID3D12CommandQueue* this_,
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists,
    const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next)
    = 0;
};

}// namespace OpenKneeboard
