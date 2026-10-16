// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <chrono>

namespace OpenKneeboard::chrono {

// In October 2026, Microsoft added several timezones (e.g. "Pacific Time
// (British Columbia)" to Windows, but did not add support for them to
// `icu.dll`, which is used by `std::chrono` to manage timezones.
// https://github.com/microsoft/STL/issues/6490
const std::chrono::time_zone* current_zone_or_utc();

}// namespace OpenKneeboard::chrono
