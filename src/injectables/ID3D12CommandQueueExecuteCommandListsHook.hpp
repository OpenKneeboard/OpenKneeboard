// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <d3d12.h>

#include <functional>
#include <memory>

namespace OpenKneeboard {

class ID3D12CommandQueueExecuteCommandListsHook final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  ID3D12CommandQueueExecuteCommandListsHook();
  ~ID3D12CommandQueueExecuteCommandListsHook();

  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<void(
      ID3D12CommandQueue* this_,
      UINT NumCommandLists,
      ID3D12CommandList* const* ppCommandLists,
      const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next)>
      onExecuteCommandLists;
  };

  void InstallHook(const Callbacks&);
  void UninstallHook();
};

}// namespace OpenKneeboard
