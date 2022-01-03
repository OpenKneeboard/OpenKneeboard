#pragma once

#include <dinput.h>
#include <winrt/base.h>

#include <vector>

namespace OpenKneeboard {

std::vector<DIDEVICEINSTANCE> GetDirectInputDevices(
  const winrt::com_ptr<IDirectInput8>& di8);

}
