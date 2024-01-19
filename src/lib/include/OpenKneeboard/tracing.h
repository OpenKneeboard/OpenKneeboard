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
#include <OpenKneeboard/scope_guard.h>

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

#define OPENKNEEBOARD_TraceLoggingWriteStartFunction(OKBTL_ACTIVITY, ...) \
  TraceLoggingWriteStart( \
    OKBTL_ACTIVITY, \
    OPENKNEEBOARD_STRINGIFY2(__FUNCTION__)##"()", \
    OPENKNEEBOARD_TraceLoggingSourceLocation(std::source_location::current()), \
    ##__VA_ARGS__)

#define OPENKNEEBOARD_TraceLoggingWriteStopFunction(OKBTL_ACTIVITY, ...) \
  TraceLoggingWriteStart( \
    OKBTL_ACTIVITY, \
    OPENKNEEBOARD_STRINGIFY2(__FUNCTION__)##"()", \
    ##__VA_ARGS__)

#define OPENKNEEBOARD_TraceLoggingScopedActivity( \
  OKBTL_ACTIVITY, OKBTL_NAME, ...) \
  TraceLoggingThreadActivity<gTraceProvider> OKBTL_ACTIVITY; \
  constexpr auto OPENKNEEBOARD_CONCAT2(OKBTL_ACTIVITY, _location) \
    = std::source_location::current(); \
  TraceLoggingWriteStart( \
    OKBTL_ACTIVITY, \
    OKBTL_NAME, \
    OPENKNEEBOARD_TraceLoggingSourceLocation( \
      OPENKNEEBOARD_CONCAT2(OKBTL_ACTIVITY, _location)), \
    ##__VA_ARGS__); \
  const OpenKneeboard::scope_guard OPENKNEEBOARD_CONCAT2( \
    OKBTL_ACTIVITY, _scopeExit)([&OKBTL_ACTIVITY]() { \
    TraceLoggingWriteStop(OKBTL_ACTIVITY, OKBTL_NAME); \
  });

#define OPENKNEEBOARD_TraceLoggingScope(OKBTL_NAME, ...) \
  OPENKNEEBOARD_TraceLoggingScopedActivity( \
    OPENKNEEBOARD_CONCAT2(okbtlsa, __COUNTER__), OKBTL_NAME, ##__VA_ARGS__)

#define OPENKNEEBOARD_TraceLoggingFunctionActivity(OKBTL_ACTIVITY, ...) \
  OPENKNEEBOARD_TraceLoggingScopedActivity( \
    OKBTL_ACTIVITY, \
    OPENKNEEBOARD_STRINGIFY2(OPENKNEEBOARD_CONCAT2(__FUNCTION__, ())), \
    ##__VA_ARGS__)

#define OPENKNEEBOARD_TraceLoggingFunction(...) \
  OPENKNEEBOARD_TraceLoggingFunctionActivity( \
    OPENKNEEBOARD_CONCAT2(okbtl_fun, __COUNTER__), ##__VA_ARGS__)

}// namespace OpenKneeboard
