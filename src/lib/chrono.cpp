// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
//
// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <OpenKneeboard/chrono.hpp>

#include <chrono>
#include <thread>

namespace OpenKneeboard::chrono {

const std::chrono::time_zone* current_zone_or_utc() {
  static std::once_flag once;
  static const std::chrono::time_zone* tz {};
  std::call_once(once, [pp = &tz] {
    try {
      *pp = std::chrono::current_zone();
    } catch (const std::exception&) {
      *pp = std::chrono::get_tzdb().locate_zone("Etc/UTC");
    }
  });
  return tz;
}

}// namespace OpenKneeboard::chrono
