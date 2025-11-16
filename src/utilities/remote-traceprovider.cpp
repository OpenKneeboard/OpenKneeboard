// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/tracing.hpp>

#include <cstdlib>

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.RemoteControl")
 * 6dafad04-3f57-55d2-e92e-7e49710d7e46
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.RemoteControl",
  (0x6dafad04, 0x3f57, 0x55d2, 0xe9, 0x2e, 0x7e, 0x49, 0x71, 0x0d, 0x7e, 0x46));

}// namespace OpenKneeboard

namespace {

TraceLoggingThreadActivity<OpenKneeboard::gTraceProvider> gActivity;

class TraceLoggingRegistration {
 public:
  TraceLoggingRegistration() {
    TraceLoggingRegister(OpenKneeboard::gTraceProvider);
    TraceLoggingWriteStart(
      gActivity,
      "Invocation",
      TraceLoggingThisExecutable(),
      TraceLoggingValue(GetCommandLineW(), "Command Line"));
  }
  ~TraceLoggingRegistration() {
    TraceLoggingWriteStop(gActivity, "Invocation");
    TraceLoggingUnregister(OpenKneeboard::gTraceProvider);
  }

  TraceLoggingRegistration(const TraceLoggingRegistration&) = delete;
  TraceLoggingRegistration(TraceLoggingRegistration&&) = delete;
  TraceLoggingRegistration& operator=(const TraceLoggingRegistration&) = delete;
  TraceLoggingRegistration& operator=(TraceLoggingRegistration&&) = delete;
};
}// namespace

static TraceLoggingRegistration gTraceLoggingRegistration;
