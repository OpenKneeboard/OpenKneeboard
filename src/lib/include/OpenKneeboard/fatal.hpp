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

#include <OpenKneeboard/config.hpp>

#include <concepts>
#include <exception>
#include <format>
#include <functional>
#include <string>
#include <variant>

typedef LONG HRESULT;

namespace std {
struct source_location;
class stacktrace_entry;
}// namespace std

namespace OpenKneeboard {

struct StackFramePointer {
  void* mValue {nullptr};

  StackFramePointer() = delete;

  constexpr StackFramePointer(std::nullptr_t) noexcept {
  }

  explicit constexpr StackFramePointer(
    std::type_identity_t<void*> value) noexcept
    : mValue(value) {
  }

  std::string to_string() const noexcept;
};
}// namespace OpenKneeboard

namespace OpenKneeboard::detail {
struct SourceLocation {
  std::string mFunctionName;
  std::string mFileName;
  size_t mLine {0};
  size_t mColumn {0};

  SourceLocation() = delete;

  SourceLocation(const std::stacktrace_entry& entry) noexcept;
  SourceLocation(StackFramePointer) noexcept;
  SourceLocation(const std::source_location& loc) noexcept;
};

struct FatalData {
  std::string mMessage;
  std::optional<SourceLocation> mBlameLocation;

  OPENKNEEBOARD_NOINLINE
  [[noreturn]]
  void fatal() const noexcept;
};

inline void break_if_debugger_present() {
  if (IsDebuggerPresent()) {
    __debugbreak();
  }
}

inline void prepare_to_fatal() {
  break_if_debugger_present();
}
}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

enum class DumpType {
  MiniDump,
  FullDump,
};
void SetDumpType(DumpType);

template <class... Ts>
OPENKNEEBOARD_NOINLINE [[noreturn]]
void fatal(std::format_string<Ts...> fmt, Ts&&... values) noexcept {
  detail::prepare_to_fatal();
  detail::FatalData {
    .mMessage = std::format(fmt, std::forward<Ts>(values)...),
  }
    .fatal();
}

template <std::convertible_to<detail::SourceLocation> TLocation, class... Ts>
OPENKNEEBOARD_NOINLINE [[noreturn]]
void fatal(
  TLocation&& blame,
  std::format_string<Ts...> fmt,
  Ts&&... values) noexcept {
  detail::prepare_to_fatal();
  detail::FatalData {
    .mMessage = std::format(fmt, std::forward<Ts>(values)...),
    .mBlameLocation = detail::SourceLocation {std::forward<TLocation>(blame)},
  }
    .fatal();
}

void fatal_with_hresult(HRESULT);
void fatal_with_exception(std::exception_ptr);

/// Hook std::terminate() and SetUnhandledExceptionFilter()
void divert_process_failure_to_fatal();

}// namespace OpenKneeboard

namespace OpenKneeboard::detail {

inline void assert_impl(bool value, std::string_view stringified) {
  if (value) [[likely]] {
    return;
  }
  ::OpenKneeboard::fatal(
    StackFramePointer {_ReturnAddress()}, "Assertion failed: {}", stringified);
}

template <class... Ts>
inline void assert_impl(
  bool value,
  std::string_view stringified,
  std::format_string<Ts...> fmt,
  Ts&&... fmtArgs) {
  if (value) [[likely]] {
    return;
  }
  ::OpenKneeboard::fatal(
    StackFramePointer {_ReturnAddress()},
    "Assertion failed ({}): {}",
    stringified,
    std::format(fmt, std::forward<Ts>(fmtArgs)...));
}

}// namespace OpenKneeboard::detail

#define OPENKNEEBOARD_ALWAYS_ASSERT(x, ...) \
  ::OpenKneeboard::detail::assert_impl(static_cast<bool>(x), #x, ##__VA_ARGS__);

// Maybe change this to depend on DEBUG in the future
#define OPENKNEEBOARD_ASSERT(x, ...) \
  OPENKNEEBOARD_ALWAYS_ASSERT(x, ##__VA_ARGS__)