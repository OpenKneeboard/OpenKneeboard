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

#include <OpenKneeboard/scope_guard.h>

#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>

namespace OpenKneeboard {

TRACELOGGING_DECLARE_PROVIDER(gTraceProvider);

#define OPENKNEEBOARD_TraceLoggingSourceLocation(loc) \
  TraceLoggingValue(loc.file_name(), "File"), \
    TraceLoggingValue(loc.line(), "Line"), \
    TraceLoggingValue(loc.function_name(), "Function")

#define OPENKNEEBOARD_TraceLoggingScopedActivity(x, ...) \
  TraceLoggingThreadActivity<gTraceProvider> okbtlta_##__COUNTER__; \
  TraceLoggingWriteStart(okbtla_##__COUNTER__, x, __VA__ARGS__); \
  const : OpenKneeboard::scope_guard okbtlasg_##__COUNTER__ { \
            [&activity = okbtlta_##__COUNTER]() { \
              TraceLoggingWriteStop(activity, x); \
            }};

}// namespace OpenKneeboard
