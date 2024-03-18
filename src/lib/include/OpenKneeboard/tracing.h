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

// clang-format off
#include <Windows.h>
// clang-format on

#include <OpenKneeboard/macros.h>

#include <exception>
#include <source_location>

#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>

namespace OpenKneeboard {

TRACELOGGING_DECLARE_PROVIDER(gTraceProvider);

#define OPENKNEEBOARD_TraceLoggingSourceLocation(loc) \
  TraceLoggingValue(loc.file_name(), "File"), \
    TraceLoggingValue(loc.line(), "Line"), \
    TraceLoggingValue(loc.function_name(), "Function")

// TraceLoggingWriteStart() requires the legacy preprocessor :(
static_assert(_MSVC_TRADITIONAL);
// Rewrite these macros if this fails, as presumably the above was fixed :)
//
// - ##__VA_ARGS__             (common vendor extension)
// + __VA_OPT__(,) __VA_ARGS__ (standard C++20)
static_assert(!OPENKNEEBOARD_VA_OPT_SUPPORTED);
// ... but we currently depend on ##__VA_ARGS__
static_assert(OPENKNEEBOARD_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION);

/** Create and automatically start and stop a named activity.
 *
 * @param OKBTL_ACTIVITY the local variable to store the activity in
 * @param OKBTL_NAME the name of the activity (C string literal)
 *
 * @see OPENKNEEBOARD_TraceLoggingScope if you don't need the local variable
 *
 * This avoids templates and `auto` and generally jumps through hoops so that it
 * is valid both inside an implementation, and in a class definition.
 */
#define OPENKNEEBOARD_TraceLoggingScopedActivity( \
  OKBTL_ACTIVITY, OKBTL_NAME, ...) \
  const std::function<void(TraceLoggingThreadActivity<gTraceProvider>&)> \
    OPENKNEEBOARD_CONCAT2(_StartImpl, OKBTL_ACTIVITY) \
    = [&, loc = std::source_location::current()]( \
        TraceLoggingThreadActivity<gTraceProvider>& activity) { \
        TraceLoggingWriteStart( \
          activity, \
          OKBTL_NAME, \
          OPENKNEEBOARD_TraceLoggingSourceLocation(loc), \
          ##__VA_ARGS__); \
      }; \
  class OPENKNEEBOARD_CONCAT2(_Impl, OKBTL_ACTIVITY) final \
    : public TraceLoggingThreadActivity<gTraceProvider> { \
   public: \
    OPENKNEEBOARD_CONCAT2(_Impl, OKBTL_ACTIVITY) \
    (decltype(OPENKNEEBOARD_CONCAT2(_StartImpl, OKBTL_ACTIVITY))& startImpl) { \
      startImpl(*this); \
    } \
    OPENKNEEBOARD_CONCAT2(~_Impl, OKBTL_ACTIVITY)() { \
      if (mAutoStop) { \
        this->Stop(); \
      } \
    } \
    void Stop() { \
      if (mStopped) [[unlikely]] { \
        OPENKNEEBOARD_BREAK; \
        return; \
      } \
      mStopped = true; \
      const auto exceptionCount = std::uncaught_exceptions(); \
      if (exceptionCount) [[unlikely]] { \
        TraceLoggingWriteStop( \
          *this, \
          OKBTL_NAME, \
          TraceLoggingValue(exceptionCount, "UncaughtExceptions")); \
      } else { \
        TraceLoggingWriteStop(*this, OKBTL_NAME); \
      } \
    } \
    void CancelAutoStop() { \
      mAutoStop = false; \
    } \
    /* Repetitive as Templates aren't permitted in local classes */ \
    void StopWithResult(int result) { \
      if (mStopped) [[unlikely]] { \
        OPENKNEEBOARD_BREAK; \
        return; \
      } \
      this->CancelAutoStop(); \
      mStopped = true; \
      TraceLoggingWriteStop( \
        *this, OKBTL_NAME, TraceLoggingValue(result, "Result")); \
    } \
    void StopWithResult(const char* result) { \
      if (mStopped) [[unlikely]] { \
        OPENKNEEBOARD_BREAK; \
        return; \
      } \
      this->CancelAutoStop(); \
      mStopped = true; \
      TraceLoggingWriteStop( \
        *this, OKBTL_NAME, TraceLoggingValue(result, "Result")); \
    } \
    void StopWithResult(const std::string& result) { \
      if (mStopped) [[unlikely]] { \
        OPENKNEEBOARD_BREAK; \
        return; \
      } \
      this->CancelAutoStop(); \
      mStopped = true; \
      TraceLoggingWriteStop( \
        *this, OKBTL_NAME, TraceLoggingValue(result.c_str(), "Result")); \
    } \
\
   private: \
    bool mStopped {false}; \
    bool mAutoStop {true}; \
  }; \
  OPENKNEEBOARD_CONCAT2(_Impl, OKBTL_ACTIVITY) \
  OKBTL_ACTIVITY {OPENKNEEBOARD_CONCAT2(_StartImpl, OKBTL_ACTIVITY)};

/** Create and automatically start and stop a named activity.
 *
 * Convenience wrapper around OPENKNEEBOARD_TraceLoggingScopedActivity
 * that generates the local variable names.
 *
 * @param OKBTL_NAME the name of the activity (C string literal)
 */
#define OPENKNEEBOARD_TraceLoggingScope(OKBTL_NAME, ...) \
  OPENKNEEBOARD_TraceLoggingScopedActivity( \
    OPENKNEEBOARD_CONCAT2(_okbtlsa, __COUNTER__), OKBTL_NAME, ##__VA_ARGS__)

#define OPENKNEEBOARD_TraceLoggingWrite(OKBTL_NAME, ...) \
  TraceLoggingWrite( \
    gTraceProvider, \
    OKBTL_NAME, \
    TraceLoggingValue(__FILE__, "File"), \
    TraceLoggingValue(__LINE__, "Line"), \
    TraceLoggingValue(__FUNCTION__, "Function"), \
    ##__VA_ARGS__)

}// namespace OpenKneeboard
