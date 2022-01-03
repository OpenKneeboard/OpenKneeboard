#include "GetDirectInputDevices.h"

using DeviceInstances = std::vector<DIDEVICEINSTANCE>;

namespace OpenKneeboard {

static BOOL CALLBACK EnumDeviceCallback(LPCDIDEVICEINSTANCE inst, LPVOID ctx) {
  auto& devices = *reinterpret_cast<DeviceInstances*>(ctx);
  devices.push_back(*inst);
  return DIENUM_CONTINUE;
}

DeviceInstances GetDirectInputDevices(
  const winrt::com_ptr<IDirectInput8>& di8) {
  DeviceInstances ret;
  di8->EnumDevices(
    DI8DEVCLASS_GAMECTRL,
    &EnumDeviceCallback,
    &ret,
    DIEDFL_ATTACHEDONLY);
  return ret;
}

}// namespace OpenKneeboard
