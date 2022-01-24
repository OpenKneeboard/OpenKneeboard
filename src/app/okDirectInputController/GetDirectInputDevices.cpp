/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
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
    DI8DEVCLASS_GAMECTRL, &EnumDeviceCallback, &ret, DIEDFL_ATTACHEDONLY);
  return ret;
}

}// namespace OpenKneeboard
