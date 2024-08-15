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

#include <shims/source_location>
#include <shims/winrt/base.h>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <format>
#include <stop_token>
#include <string>

namespace OpenKneeboard {

void dprint(std::string_view s);
void dprint(std::wstring_view s);

template <typename... Args>
  requires(sizeof...(Args) >= 1)
void dprintf(std::format_string<Args...> fmt, Args&&... args) {
  dprint(std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
  requires(sizeof...(Args) >= 1)
void dprintf(std::wformat_string<Args...> fmt, Args&&... args) {
  dprint(std::format(fmt, std::forward<Args>(args)...));
}

/** Utility for logging object lifetime.
 *
 * Example usage:
 *
 * ```
 * const auto lock = DPrintLifetime { std::unique_lock { (*p->mDXR) }, "Lock" };
 * ```
 *
 * Note this will log at runtime in release builds, so calls should probably not
 * be committed.
 */
template <class T>
class DPrintLifetime final {
 public:
  DPrintLifetime() = delete;
  DPrintLifetime(
    std::string_view what,
    T&& it,
    const std::source_location& loc = std::source_location::current())
    : mValue(std::move(it)),
      mWhat(what),
      mLoc(loc),
      mUncaughtExceptions(std::uncaught_exceptions()) {
    dprintf(
      "{} in {}() @ {}:{}",
      mWhat,
      loc.function_name(),
      loc.file_name(),
      loc.line());
  }

  ~DPrintLifetime() {
    dprintf(
      "~{} with {} new exceptions in {}() @ {}:{}",
      mWhat,
      std::uncaught_exceptions() - mUncaughtExceptions,
      mLoc.function_name(),
      mLoc.file_name(),
      mLoc.line());
  }

  operator T&() noexcept {
    return mValue;
  }

  operator const T&() const noexcept {
    return mValue;
  }

 private:
  const T mValue;
  const std::string mWhat;
  const std::source_location mLoc;
  const int mUncaughtExceptions;
};

struct DPrintSettings {
  enum class ConsoleOutputMode {
    NEVER,
    ALWAYS,
  };

  std::string prefix = "OpenKneeboard";
  ConsoleOutputMode consoleOutput = ConsoleOutputMode::NEVER;

  static void Set(const DPrintSettings&);
};

/**  If you change this structure, you *MUST* also change the version
 * in `GetDPrintResourceName()`.
 *
 * USE DEFINED-SIZE FIELDS ONLY - THIS STRUCT MUST BE
 * COMPATIBLE BETWEEN 32-BIT AND 64-BIT BINARIES.
 */
struct DPrintMessageHeader {
  DWORD mProcessID = 0;
  wchar_t mExecutable[MAX_PATH] {};
  wchar_t mPrefix[MAX_PATH] {};
};
static_assert(sizeof(DPrintMessageHeader) % sizeof(wchar_t) == 0);
static_assert(std::is_standard_layout_v<DPrintMessageHeader>);

/* If you change this structure, you *MUST* also change the version
 * in `GetDPrintResourceName()`
 *
 * USE DEFINED-SIZE FIELDS ONLY - THIS STRUCT MUST BE
 * COMPATIBLE BETWEEN 32-BIT AND 64-BIT BINARIES.
 */
struct DPrintMessage {
 public:
  using PortableSize = uint64_t;
  static constexpr PortableSize StructSize = 4096;
  static constexpr PortableSize MaxMessageLength
    = (StructSize - (sizeof(DPrintMessageHeader) + sizeof(PortableSize)))
    / sizeof(wchar_t);

  DPrintMessageHeader mHeader {};
  wchar_t mMessage[MaxMessageLength];
  PortableSize mMessageLength {};
};
static_assert(std::is_standard_layout_v<DPrintMessage>);
static_assert(sizeof(DPrintMessage) == DPrintMessage::StructSize);

class DPrintReceiver {
 public:
  DPrintReceiver();
  ~DPrintReceiver();

  bool IsUsable() const;
  void Run(std::stop_token);

 protected:
  virtual void OnMessage(const DPrintMessage&) = 0;

 private:
  winrt::handle mMutex;
  winrt::handle mMapping;
  winrt::handle mBufferReadyEvent;
  winrt::handle mDataReadyEvent;
  DPrintMessage* mSHM = nullptr;

  bool mUsable = false;
};

#define OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL( \
  source_location, message, ...) \
  { \
    dprintf("FATAL: " message " @ {}", ##__VA_ARGS__, source_location); \
    TraceLoggingWrite(gTraceProvider, "FATAL"); \
    OPENKNEEBOARD_FATAL; \
  }

/** Use this if something is so wrong that we're almost certainly going to
 * crash.
 *
 * Crashing earlier is better than crashing later, as we get more usable
 * debugging information.
 *
 * @see `OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL` if you need to provide a
 * source location.
 */
#define OPENKNEEBOARD_LOG_AND_FATAL(message, ...) \
  OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL( \
    std::source_location::current(), message, ##__VA_ARGS__)

}// namespace OpenKneeboard
