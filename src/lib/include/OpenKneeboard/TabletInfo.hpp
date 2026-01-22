// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <cinttypes>
#include <string>

namespace OpenKneeboard {

struct TabletInfo final {
  float mMaxX {};
  float mMaxY {};
  uint32_t mMaxPressure {};
  std::string mDeviceName;
  std::string mDevicePersistentID;
};

}// namespace OpenKneeboard
