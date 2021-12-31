#pragma once

#include <d3d11.h>
#include <winrt/base.h>

namespace OpenKneeboard {

class D3D11DeviceHook final {
  public:
    D3D11DeviceHook();
    ~D3D11DeviceHook();
    winrt::com_ptr<ID3D11Device> MaybeGet();
};

}
