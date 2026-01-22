// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <wil/registry.h>

#include <expected>

namespace OpenKneeboard::Registry {

inline std::expected<::wil::unique_hkey, HRESULT> open_unique_key(
  HKEY key,
  _In_opt_ PCWSTR subKey,
  ::wil::reg::key_access access = ::wil::reg::key_access::read) {
  wil::unique_hkey ret;
  const auto result =
    wil::reg::open_unique_key_nothrow(key, subKey, ret, access);
  if (SUCCEEDED(result)) {
    return ret;
  }
  return std::unexpected {result};
}

}// namespace OpenKneeboard::Registry
