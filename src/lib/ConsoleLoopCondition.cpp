// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/ConsoleLoopCondition.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <Windows.h>

#include <objbase.h>

namespace OpenKneeboard {

static winrt::handle gExitEvent;

static BOOL WINAPI ExitHandler(DWORD ignored) {
  CoUninitialize();
  SetEvent(gExitEvent.get());
  return TRUE;
}

class ConsoleLoopCondition::Impl final {
 public:
  winrt::handle mTimerEvent {};
};

ConsoleLoopCondition::ConsoleLoopCondition() : p(std::make_shared<Impl>()) {
  gExitEvent = Win32::or_throw::CreateEvent(nullptr, false, false, nullptr);
  p->mTimerEvent = Win32::or_throw::CreateWaitableTimer(nullptr, true, nullptr);
  SetConsoleCtrlHandler(&ExitHandler, true);
}

ConsoleLoopCondition::~ConsoleLoopCondition() {
  SetConsoleCtrlHandler(&ExitHandler, false);
  gExitEvent.close();
}

namespace {
typedef std::chrono::
  duration<int64_t, std::ratio_multiply<std::hecto, std::nano>>
    FILETIME_RESOLUTION;
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

  HANDLE handles[] = {gExitEvent.get(), p->mTimerEvent.get()};
  auto res = WaitForMultipleObjects(
    sizeof(handles) / sizeof(handles[0]), handles, false, INFINITE);
  if (handles[res - WAIT_OBJECT_0] == gExitEvent.get()) {
    CancelWaitableTimer(p->mTimerEvent.get());
    return false;
  }
  return true;
}

}// namespace OpenKneeboard
