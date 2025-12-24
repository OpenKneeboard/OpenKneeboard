// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/ConsoleLoopCondition.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <Windows.h>
#include <objbase.h>

namespace OpenKneeboard {

namespace {
auto& ExitEvent() {
  static winrt::handle gExitEvent;
  return gExitEvent;
}

BOOL WINAPI ExitHandler(DWORD ignored) {
  CoUninitialize();
  SetEvent(ExitEvent().get());
  return TRUE;
}

using FILETIME_RESOLUTION
  = std::chrono::duration<int64_t, std::ratio_multiply<std::hecto, std::nano>>;
}// namespace

class ConsoleLoopCondition::Impl final {
 public:
  winrt::handle mTimerEvent {};
};

ConsoleLoopCondition::ConsoleLoopCondition() : p(std::make_shared<Impl>()) {
  ExitEvent() = Win32::or_throw::CreateEvent(nullptr, false, false, nullptr);
  p->mTimerEvent = Win32::or_throw::CreateWaitableTimer(nullptr, true, nullptr);
  SetConsoleCtrlHandler(&ExitHandler, true);
}

ConsoleLoopCondition::~ConsoleLoopCondition() {
  SetConsoleCtrlHandler(&ExitHandler, false);
  ExitEvent().close();
}

bool ConsoleLoopCondition::Sleep(
  const std::chrono::steady_clock::duration& delay) {
  int64_t expiration {};
  GetSystemTimeAsFileTime((FILETIME*)&expiration);
  expiration += std::chrono::duration_cast<FILETIME_RESOLUTION>(delay).count();
  SetWaitableTimer(
    p->mTimerEvent.get(),
    (LARGE_INTEGER*)&expiration,
    0,
    nullptr,
    nullptr,
    false);

  const auto event = ExitEvent().get();
  HANDLE handles[] = {event, p->mTimerEvent.get()};
  auto res = WaitForMultipleObjects(
    sizeof(handles) / sizeof(handles[0]), handles, false, INFINITE);
  if (handles[res - WAIT_OBJECT_0] == event) {
    CancelWaitableTimer(p->mTimerEvent.get());
    return false;
  }
  return true;
}

}// namespace OpenKneeboard
