#include "ID3D12CommandQueueExecuteCommandListsHook.h"

#include <OpenKneeboard/dprint.h>
#include <winrt/base.h>

#include "d3d12-offsets.h"
#include "detours-ext.h"

#pragma comment(lib,"D3D12.lib")

namespace OpenKneeboard {

namespace {
ID3D12CommandQueueExecuteCommandListsHook* gHook = nullptr;
decltype(&ID3D12CommandQueue::ExecuteCommandLists)
  Real_ID3D12CommandQueue_ExecuteCommandLists
  = nullptr;
}// namespace

struct ID3D12CommandQueueExecuteCommandListsHook::Impl {
  virtual void __stdcall Hooked_ExecuteCommandLists(
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists);
};

ID3D12CommandQueueExecuteCommandListsHook::
  ID3D12CommandQueueExecuteCommandListsHook() {
  dprintf("{} {:#018x}", __FUNCTION__, (int64_t)this);
  if (gHook) {
    throw std::logic_error(
      "Only one ID3D12CommandQueueExecuteCommandList at a time!");
  }
  gHook = this;
}

ID3D12CommandQueueExecuteCommandListsHook::
  ~ID3D12CommandQueueExecuteCommandListsHook() {
  dprintf("{} {:#018x}", __FUNCTION__, (int64_t)this);
  this->UninstallHook();
}

void ID3D12CommandQueueExecuteCommandListsHook::InitWithVTable() {
  DetourTransaction dtAndConcurrencyBlock;

  winrt::com_ptr<ID3D12Device> device;
  D3D12CreateDevice(
    nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.put()));
  if (!device) {
    return;
  }
  D3D12_COMMAND_QUEUE_DESC cqDesc {
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
    D3D12_COMMAND_QUEUE_FLAG_NONE,
    0};
  winrt::com_ptr<ID3D12CommandQueue> cq;
  device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(cq.put()));

  auto fpp
    = reinterpret_cast<void**>(&Real_ID3D12CommandQueue_ExecuteCommandLists);
  *fpp = VTable_Lookup_ID3D12CommandQueue_ExecuteCommandLists(cq.get());
  auto err = DetourAttach(
    fpp, std::bit_cast<void*>(&Impl::Hooked_ExecuteCommandLists));
  if (err == 0) {
    dprintf(" - hooked ID3D12CommandQueue::ExecuteCommandLists().");
  } else {
    dprintf(
      " - failed to hook ID3D12CommandQueue::ExecuteCommandLists(): {}", err);
  }
}

void ID3D12CommandQueueExecuteCommandListsHook::UninstallHook() {
  if (gHook != this) {
    return;
  }

  DetourTransaction dt;
  DetourDetach(
    reinterpret_cast<void**>(&Real_ID3D12CommandQueue_ExecuteCommandLists),
    std::bit_cast<void*>(&Impl::Hooked_ExecuteCommandLists));
}

void __stdcall ID3D12CommandQueueExecuteCommandListsHook::Impl::
  Hooked_ExecuteCommandLists(
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists) {
  auto this_ = reinterpret_cast<ID3D12CommandQueue*>(this);
  if (gHook) {
    gHook->OnID3D12CommandQueue_ExecuteCommandLists(
      this_,
      NumCommandLists,
      ppCommandLists,
      Real_ID3D12CommandQueue_ExecuteCommandLists);
  }

  std::invoke(
    Real_ID3D12CommandQueue_ExecuteCommandLists,
    this_,
    NumCommandLists,
    ppCommandLists);
}

}// namespace OpenKneeboard
