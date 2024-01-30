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

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/tracing.h>

#include <shims/source_location>
#include <shims/winrt/base.h>

#include <format>
#include <stop_token>
#include <string>

namespace OpenKneeboard {

void dprint(std::string_view s);
void dprint(std::wstring_view s);

// TODO: cleanup, once GitHub Actions updates their worker
namespace detail {
#if __cpp_lib_format >= 202207L
template <class... T>
using format_string = std::format_string<T...>;
template <class... T>
using wformat_string = std::wformat_string<T...>;
#else
template <class... T>
using format_string = std::_Fmt_string<T...>;
template <class... T>
using wformat_string = std::_Fmt_wstring<T...>;
#endif
}// namespace detail

template <typename... Args>
void dprintf(detail::format_string<Args...> fmt, Args&&... args) {
  static_assert(sizeof...(args) > 0, "Use dprint() when no variables");
  dprint(std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void dprintf(detail::wformat_string<Args...> fmt, Args&&... args) {
  static_assert(sizeof...(args) > 0, "Use dprint() when no variables");
  dprint(std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void traceprint(detail::format_string<Args...> fmt, Args&&... args) {
  TraceLoggingWrite(
    gTraceProvider,
    "traceprint",
    TraceLoggingValue(
      std::format(fmt, std::forward<Args>(args)...).c_str(), "Message"));
}

template <typename... Args>
void traceprint(detail::wformat_string<Args...> fmt, Args&&... args) {
  TraceLoggingWrite(
    gTraceProvider,
    "traceprint",
    TraceLoggingValue(
      std::format(fmt, std::forward<Args>(args)...).c_str(), "Message"));
}

struct DPrintSettings {
  enum class ConsoleOutputMode {
    NEVER,
    ALWAYS,
  };

  std::string prefix = "OpenKneeboard";
  ConsoleOutputMode consoleOutput = ConsoleOutputMode::NEVER;

  static void Set(const DPrintSettings&);
};

#pragma pack(push)
struct DPrintMessageHeader {
  DWORD mProcessID = 0;
  wchar_t mExecutable[MAX_PATH];
  wchar_t mPrefix[MAX_PATH];
};
static_assert(sizeof(DPrintMessageHeader) % sizeof(wchar_t) == 0);

struct DPrintMessage : public DPrintMessageHeader {
 private:
  static constexpr auto HeaderSize = sizeof(DPrintMessageHeader);
  static constexpr auto MaxMessageSize = 4096 - HeaderSize;

 public:
  static constexpr auto MaxMessageLength = MaxMessageSize / sizeof(wchar_t);
  wchar_t mMessage[MaxMessageLength];
};
#pragma pack(pop)

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
