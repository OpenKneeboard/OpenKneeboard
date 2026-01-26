// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <OpenKneeboard/task.hpp>
#include <OpenKneeboard/task/resume_on_signal.hpp>

#include <wil/com.h>
#include <wil/resource.h>

#include <print>
#include <ranges>

namespace OpenKneeboard {
/* PS > [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.App")
 * cc76597c-1041-5d57-c8ab-92cf9437104a
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.App",
  (0xcc76597c, 0x1041, 0x5d57, 0xc8, 0xab, 0x92, 0xcf, 0x94, 0x37, 0x10, 0x4a));
}// namespace OpenKneeboard

using namespace OpenKneeboard;

struct timers {
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

  struct entry {
    time_point when;
    std::string_view label;
    std::source_location loc;

    constexpr auto operator-(const entry& other) const noexcept {
      return std::chrono::duration<double>(when - other.when);
    }
  };

  timers(const std::source_location& loc = std::source_location::current()) {
    entries.push_back({std::chrono::steady_clock::now(), "start", loc});
  }

  void mark(
    const std::string_view label,
    const std::source_location& loc = std::source_location::current()) {
    entries.push_back({std::chrono::steady_clock::now(), label, loc});
  }

  void dump() {
    std::println("Total time: {:.04}", entries.back() - entries.front());

    for (auto&& [start, end]: std::views::pairwise(entries)) {
      std::println(
        "{}: {:.04} ({}:{} -> {}:{})",
        end.label,
        end - start,
        start.loc.file_name(),
        start.loc.line(),
        end.loc.file_name(),
        end.loc.line());
    }
  }

  std::vector<entry> entries;
};

wil::unique_event TestFinished;

fire_and_forget do_test() {
  timers testTimers;

  std::println("resume_background test");
  for (int i = 0; i < 1'000'000; ++i) {
    if (i % 10'000 == 0) {
      std::println("iteration: {}", i);
    }
    std::atomic<std::size_t> deleteCount = 0;
    co_await [](auto* p) -> task<void> {
      const auto setter = scope_exit([p] { ++*p; });
      co_await winrt::resume_background();
      co_return;
    }(&deleteCount);
    const auto count = deleteCount.load();
    OPENKNEEBOARD_ASSERT(count == 1);
  }
  testTimers.mark("resume_background");

  std::println("immediate return test");
  for (int i = 0; i < 1'000'000; ++i) {
    if (i % 10'000 == 0) {
      std::println("iteration: {}", i);
    }
    std::atomic<std::size_t> deleteCount = 0;
    co_await [](auto* p) -> task<void> {
      const auto setter = scope_exit([p] { ++*p; });
      co_return;
    }(&deleteCount);
    const auto count = deleteCount.load();
    OPENKNEEBOARD_ASSERT(count == 1);
  }
  testTimers.mark("immediate return");

  std::println("resume_background with delay move");
  for (int i = 0; i < 1'000'000; ++i) {
    if (i % 10'000 == 0) {
      std::println("iteration: {}", i);
    }
    std::atomic<std::size_t> deleteCount = 0;
    auto coro = [](auto* p) -> task<void> {
      const auto setter = scope_exit([p] { ++*p; });
      co_await winrt::resume_background();
      co_return;
    }(&deleteCount);
    std::this_thread::yield();
    co_await std::move(coro);
    const auto count = deleteCount.load();
    OPENKNEEBOARD_ASSERT(count == 1);
  }
  testTimers.mark("resume_background with delay move");

  wil::unique_event e;
  e.create();

  std::println("delayed ready");
  for (int i = 0; i < 1'000'000; ++i) {
    if (i % 10'000 == 0) {
      std::println("iteration: {}", i);
    }
    std::atomic<std::size_t> deleteCount = 0;
    auto coro = [](auto handle, auto* p) -> task<void> {
      const auto setter = scope_exit([p] { ++*p; });
      co_await resume_on_signal(handle);
      co_return;
    }(e.get(), &deleteCount);
    std::this_thread::yield();
    e.SetEvent();
    co_await std::move(coro);
    const auto count = deleteCount.load();
    OPENKNEEBOARD_ASSERT(count == 1);
    e.ResetEvent();
  }
  testTimers.mark("delayed ready");

  std::println("delayed exception");
  for (int i = 0; i < 1'000'000; ++i) {
    if (i % 10'000 == 0) {
      std::println("iteration: {}", i);
    }
    std::atomic<std::size_t> deleteCount = 0;
    auto coro = [](auto handle, auto index, auto p) -> task<void> {
      const auto setter = scope_exit([p] { ++*p; });
      co_await resume_on_signal(handle);
      throw std::runtime_error(std::format("testy mctestface {}", index));
    }(e.get(), i, &deleteCount);
    std::this_thread::yield();
    e.SetEvent();
    try {
      co_await std::move(coro);
      fatal("exception not rethrown");
    } catch (const std::runtime_error& ex) {
      const auto count = deleteCount.load();
      OPENKNEEBOARD_ASSERT(count == 1);
      const auto msg = ex.what();
      OPENKNEEBOARD_ASSERT(msg == std::format("testy mctestface {}", i));
    }
    e.ResetEvent();
  }
  testTimers.mark("delayed exception");
  testTimers.dump();
  TestFinished.SetEvent();
  co_return;
}

int main() {
  TraceLoggingRegister(gTraceProvider);
  const auto tlUnregister =
    scope_exit([] { TraceLoggingUnregister(gTraceProvider); });
  const auto com = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
  wil::unique_mta_usage_cookie mta;
  CoIncrementMTAUsage(mta.put());

  TestFinished.create();
  HANDLE handles[] = {TestFinished.get()};

  bool running = true;
  do_test();
  while (running) {
    // Wait for either the event to be signaled OR a message to arrive in the
    // queue
    DWORD result = MsgWaitForMultipleObjects(
      static_cast<DWORD>(std::size(handles)),
      handles,
      FALSE,// Wait for ANY handle or message
      INFINITE,// Timeout
      QS_ALLINPUT// Wake up for any type of message/input
    );

    if (result == WAIT_OBJECT_0) {
      // TestFinished was signaled
      running = false;
    } else if (result == WAIT_OBJECT_0 + std::size(handles)) {
      // A message is waiting in the queue; process it
      MSG msg;
      while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          running = false;
          break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    } else if (result == WAIT_FAILED) {
      THROW_LAST_ERROR();
    }
  }

  return 0;
}
