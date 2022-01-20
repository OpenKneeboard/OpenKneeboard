#include "ID3D12CommandQueueExecuteCommandListsHook.h"

#include <OpenKneeboard/dprint.h>
#include <winrt/base.h>

#include "d3d12-offsets.h"
#include "detours-ext.h"

#pragma comment(lib, "D3D12.lib")

namespace OpenKneeboard {

namespace {
decltype(&ID3D12CommandQueue::ExecuteCommandLists)
  Next_ID3D12CommandQueue_ExecuteCommandLists
  = nullptr;
}// namespace

struct ID3D12CommandQueueExecuteCommandListsHook::Impl {
  void __stdcall Hooked_ExecuteCommandLists(
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists);

  Impl(const Callbacks&);

  void UninstallHook();

 private:
  Callbacks mCallbacks;
  static Impl* gInstance;
};
ID3D12CommandQueueExecuteCommandListsHook::Impl*
  ID3D12CommandQueueExecuteCommandListsHook::Impl::gInstance
  = nullptr;

ID3D12CommandQueueExecuteCommandListsHook::
  ID3D12CommandQueueExecuteCommandListsHook() {
}

ID3D12CommandQueueExecuteCommandListsHook::
  ~ID3D12CommandQueueExecuteCommandListsHook() {
  this->UninstallHook();
}

void ID3D12CommandQueueExecuteCommandListsHook::InstallHook(
  const Callbacks& cb) {
  if (!cb.onExecuteCommandLists) {
    throw std::logic_error(
      "ID3D12CommandQueueExecuteCommandListsHook without onExecuteCommandLists "
      "callback");
  }
  p = std::make_unique<Impl>(cb);
}

ID3D12CommandQueueExecuteCommandListsHook::Impl::Impl(const Callbacks& cb)
  : mCallbacks(cb) {
  if (gInstance) {
    throw std::logic_error(
      "Only one ID3D12CommandQueueExecuteCommandListsHook at a time");
  }
  gInstance = this;

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
    = reinterpret_cast<void**>(&Next_ID3D12CommandQueue_ExecuteCommandLists);
  *fpp = VTable_Lookup_ID3D12CommandQueue_ExecuteCommandLists(cq.get());
  auto err = DetourSingleAttach(
    fpp, std::bit_cast<void*>(&Impl::Hooked_ExecuteCommandLists));
  if (err == 0) {
    dprintf(" - hooked ID3D12CommandQueue::ExecuteCommandLists().");
  } else {
    dprintf(
      " - failed to hook ID3D12CommandQueue::ExecuteCommandLists(): {}", err);
  }

  if (cb.onHookInstalled) {
    cb.onHookInstalled();
  }
}

void ID3D12CommandQueueExecuteCommandListsHook::UninstallHook() {
  if (p) {
    p->UninstallHook();
  }
}

void ID3D12CommandQueueExecuteCommandListsHook::Impl::UninstallHook() {
  if (gInstance != this) {
    return;
  }

  DetourSingleDetach(
    reinterpret_cast<void**>(&Next_ID3D12CommandQueue_ExecuteCommandLists),
    std::bit_cast<void*>(&Impl::Hooked_ExecuteCommandLists));
  gInstance = nullptr;
}

void __stdcall ID3D12CommandQueueExecuteCommandListsHook::Impl::
  Hooked_ExecuteCommandLists(
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists) {
  auto this_ = reinterpret_cast<ID3D12CommandQueue*>(this);
  if (gInstance) [[likely]] {
    gInstance->mCallbacks.onExecuteCommandLists(
      this_,
      NumCommandLists,
      ppCommandLists,
      Next_ID3D12CommandQueue_ExecuteCommandLists);
  }

  std::invoke(
    Next_ID3D12CommandQueue_ExecuteCommandLists,
    this_,
    NumCommandLists,
    ppCommandLists);
}

}// namespace OpenKneeboard
