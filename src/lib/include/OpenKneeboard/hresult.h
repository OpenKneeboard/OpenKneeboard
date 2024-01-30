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
#pragma once

#include <OpenKneeboard/dprint.h>

#include <shims/winrt/base.h>

#include <format>

template <class CharT>
struct std::formatter<winrt::hresult, CharT>
  : std::formatter<std::basic_string_view<CharT>, CharT> {
  template <class FormatContext>
  auto format(const winrt::hresult& hresult, FormatContext& fc) const {
    char* message = nullptr;
    FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      hresult,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<char*>(&message),
      0,
      nullptr);
    std::basic_string<CharT> converted;
    if (!message) {
      converted = std::format("{:#010x}", static_cast<uint32_t>(hresult.value));
      OPENKNEEBOARD_BREAK;
    } else {
      converted = std::format(
        "{:#010x} (\"{}\")", static_cast<uint32_t>(hresult.value), converted);
    }

    return std::formatter<std::basic_string_view<CharT>, CharT>::format(
      std::basic_string_view<CharT> {converted}, fc);
  }
};

namespace OpenKneeboard {

inline void check_hresult(
  HRESULT code,
  const std::source_location& loc = std::source_location::current()) {
  if (FAILED(code)) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("HRESULT {}", winrt::hresult {code});
  }
}

}// namespace OpenKneeboard