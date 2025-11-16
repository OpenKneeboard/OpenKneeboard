// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/dprint.hpp>

#include <shims/winrt/base.h>

#include <format>

namespace OpenKneeboard {

inline void check_hresult(
  HRESULT code,
  const std::source_location& loc = std::source_location::current()) {
  if (FAILED(code)) [[unlikely]] {
    fatal(loc, "HRESULT {}", winrt::hresult {code});
  }
}

}// namespace OpenKneeboard