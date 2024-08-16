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

#include <format>
#include <optional>
#include <source_location>
#include <stacktrace>

namespace OpenKneeboard::detail {

struct FatalData {
  std::string mMessage;
  std::optional<std::source_location> mBlameLocation;

  [[noreturn, msvc::noinline]]
  void fatal() const noexcept;
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

template <class... Ts>
[[noreturn, msvc::noinline]]
void fatal(std::format_string<Ts...> fmt, Ts&&... values) noexcept {
  detail::FatalData {
    .mMessage = std::format(fmt, std::forward<Ts>(values)...),
  }
    .fatal();
}

template <class... Ts>
[[noreturn, msvc::noinline]]
void fatal(
  const std::source_location& blame,
  std::format_string<Ts...> fmt,
  Ts&&... values) noexcept {
  detail::FatalData {
    .mMessage = std::format(fmt, std::forward<Ts>(values)...),
    .mBlameLocation = blame,
  }
    .fatal();
}

/// Hook std::terminate() and SetUnhandledExceptionFilter()
void divert_process_failure_to_fatal();

}// namespace OpenKneeboard