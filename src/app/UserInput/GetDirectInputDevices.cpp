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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/GetDirectInputDevices.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/utf8.h>

using DeviceInstances = std::vector<DIDEVICEINSTANCE>;

namespace OpenKneeboard {

static BOOL CALLBACK EnumDeviceCallback(LPCDIDEVICEINSTANCE inst, LPVOID ctx) {
  auto devType = inst->dwDevType & 0xff;
  // vjoystick devices self-report as 6DOF 1st-person controllers
  if (
    devType == DI8DEVTYPE_JOYSTICK || devType == DI8DEVTYPE_GAMEPAD
    || devType == DI8DEVTYPE_FLIGHT || devType == DI8DEVTYPE_1STPERSON
    || devType == DI8DEVTYPE_KEYBOARD) {
    auto& devices = *reinterpret_cast<DeviceInstances*>(ctx);
    devices.push_back(*inst);
  } else {
    dprintf(
      "Skipping DirectInput device '{}' with filtered device type {:#010x}",
      to_utf8(inst->tszInstanceName),
      inst->dwDevType);
  }
  return DIENUM_CONTINUE;
}

DeviceInstances GetDirectInputDevices(IDirectInput8W* di) {
  DeviceInstances ret;
  di->EnumDevices(
    DI8DEVCLASS_ALL, &EnumDeviceCallback, &ret, DIEDFL_ATTACHEDONLY);
  return ret;
}

}// namespace OpenKneeboard
