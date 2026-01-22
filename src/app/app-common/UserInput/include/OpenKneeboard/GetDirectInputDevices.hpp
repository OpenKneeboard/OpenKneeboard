// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <vector>

#include <dinput.h>

namespace OpenKneeboard {

std::vector<DIDEVICEINSTANCE> GetDirectInputDevices(
  IDirectInput8W* di,
  bool includeMice);

}
