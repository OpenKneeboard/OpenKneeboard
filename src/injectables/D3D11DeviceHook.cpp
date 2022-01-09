#include "D3D11DeviceHook.h"

#include <OpenKneeboard/dprint.h>

#include "d3d11-offsets.h"
#include "detours-ext.h"

namespace OpenKneeboard {

D3D11DeviceHook::D3D11DeviceHook() : IDXGISwapChainPresentHook() {
}

winrt::com_ptr<ID3D11Device> D3D11DeviceHook::MaybeGet() {
  return mD3D;
}

D3D11DeviceHook::~D3D11DeviceHook() {
}

HRESULT D3D11DeviceHook::OnPresent(
  UINT syncInterval,
  UINT flags,
  IDXGISwapChain* swapChain,
  decltype(&IDXGISwapChain::Present) next) {
  swapChain->GetDevice(IID_PPV_ARGS(mD3D.put()));
  dprintf("Got device at {:#010x} from {}", (intptr_t)mD3D.get(), __FUNCTION__);
  auto ret = std::invoke(next, swapChain, syncInterval, flags);
  Unhook();
  return ret;
}

}// namespace OpenKneeboard
