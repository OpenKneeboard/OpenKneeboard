// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/config.hpp>

#include <concepts>
#include <exception>
#include <format>
#include <functional>
#include <span>
#include <stacktrace>
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

  constexpr StackFramePointer(std::nullptr_t) noexcept {}

  explicit constexpr StackFramePointer(std::stacktrace_entry entry) noexcept
    : mValue(std::bit_cast<void*>(entry)) {}

  explicit constexpr StackFramePointer(
    std::type_identity_t<void*> value) noexcept
    : mValue(value) {}

  auto operator->() const noexcept {
    struct wrapper_t {
      std::stacktrace_entry mValue;
      auto operator->() const noexcept { return &mValue; }
    };
    return wrapper_t(std::bit_cast<std::stacktrace_entry>(mValue));
  }

  OPENKNEEBOARD_FORCEINLINE
  static StackFramePointer caller(size_t skip = 0) {
    if (skip == 0) {
      return StackFramePointer {_ReturnAddress()};
    } else {
      void* ptr {nullptr};
      while (!CaptureStackBackTrace(skip + 1, 1, &ptr, nullptr)) {
        // retry. Undocumented reliability issues:
        // https://github.com/microsoft/STL/issues/3889
      }
      return StackFramePointer {ptr};
    }
  }

  std::string to_string() const noexcept;
};

struct StackTrace {
  OPENKNEEBOARD_FORCEINLINE
  static StackTrace Current(std::size_t skip = 0) noexcept;

  friend std::ostream& operator<<(std::ostream&, const StackTrace&);

  inline StackFramePointer at(std::size_t pos) const {
    if (pos >= mSize) [[unlikely]] {
      throw std::out_of_range(
        std::format("Requested stack frame {} of {}", pos, mSize));
    }
    return GetEntries()[pos];
  }

  static StackTrace GetForMostRecentException();
  static void SetForNextException(const StackTrace&);

 private:
  std::shared_ptr<void> mData;
  std::size_t mSize {0};

  std::span<StackFramePointer> GetEntries() const;
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

inline void prepare_to_fatal() { break_if_debugger_present(); }
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
  const auto message =
    std::vformat(fmt.get(), std::make_format_args(values...));
  detail::prepare_to_fatal();
  detail::FatalData {
    .mMessage = message,
  }
    .fatal();
}

template <std::convertible_to<detail::SourceLocation> TLocation, class... Ts>
OPENKNEEBOARD_NOINLINE [[noreturn]]
void fatal(
  TLocation&& blame,
  std::format_string<Ts...> fmt,
  Ts&&... values) noexcept {
  auto message = std::format(fmt, std::forward<Ts>(values)...);
  detail::prepare_to_fatal();
  detail::FatalData {
    .mMessage = std::move(message),
    .mBlameLocation = detail::SourceLocation {std::forward<TLocation>(blame)},
  }
    .fatal();
}

void fatal_with_hresult(HRESULT);
void fatal_with_exception(std::exception_ptr);

/// Hook std::terminate() and SetUnhandledExceptionFilter()
void divert_process_failure_to_fatal();

/** Useful if `noexcept` is unusable.
 *
 * For example, CEF requires interfaces be implemented that do not throw
 * exceptions, but does not declare the functions to be noexcept.
 */
class FatalOnUncaughtExceptions final {
 public:
  FatalOnUncaughtExceptions();
  ~FatalOnUncaughtExceptions();

  FatalOnUncaughtExceptions(const FatalOnUncaughtExceptions&) = delete;
  FatalOnUncaughtExceptions(FatalOnUncaughtExceptions&&) = delete;
  FatalOnUncaughtExceptions& operator=(const FatalOnUncaughtExceptions&) =
    delete;
  FatalOnUncaughtExceptions& operator=(FatalOnUncaughtExceptions&&) = delete;

 private:
  int mUncaughtExceptions;
};

}// namespace OpenKneeboard

namespace OpenKneeboard::detail {

inline constexpr void assert_impl(bool value, std::string_view stringified) {
  if (value) [[likely]] {
    return;
  }
  ::OpenKneeboard::fatal(
    StackFramePointer {_ReturnAddress()}, "Assertion failed: {}", stringified);
}

template <class... Ts>
inline constexpr void assert_impl(
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
