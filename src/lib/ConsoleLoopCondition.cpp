#include "OpenKneeboard/ConsoleLoopCondition.h"

#include <Windows.h>
#include <objbase.h>

namespace OpenKneeboard {

static HANDLE gExitEvent = INVALID_HANDLE_VALUE;

static BOOL WINAPI ExitHandler(DWORD ignored) {
  CoUninitialize();
  SetEvent(gExitEvent);
  return TRUE;
}

class ConsoleLoopCondition::Impl final {
 public:
  HANDLE TimerEvent = nullptr;
};

ConsoleLoopCondition::ConsoleLoopCondition() : p(std::make_shared<Impl>()) {
  gExitEvent = CreateEvent(nullptr, false, false, nullptr);
  p->TimerEvent = CreateWaitableTimer(nullptr, true, nullptr);
  SetConsoleCtrlHandler(&ExitHandler, true);
}

ConsoleLoopCondition::~ConsoleLoopCondition() {
  SetConsoleCtrlHandler(&ExitHandler, false);
  CloseHandle(p->TimerEvent);
  CloseHandle(gExitEvent);
}

namespace {
typedef std::chrono::
  duration<int64_t, std::ratio_multiply<std::hecto, std::nano>>
    FILETIME_RESOLUTION;
}

bool ConsoleLoopCondition::Sleep(
  const std::chrono::steady_clock::duration& delay) {
  int64_t expiration;
  GetSystemTimeAsFileTime((FILETIME*)&expiration);
  expiration += std::chrono::duration_cast<FILETIME_RESOLUTION>(delay).count();
  SetWaitableTimer(
    p->TimerEvent, (LARGE_INTEGER*)&expiration, 0, nullptr, nullptr, false);

  HANDLE handles[] = {gExitEvent, p->TimerEvent};
  auto res = WaitForMultipleObjects(
    sizeof(handles) / sizeof(handles[0]), handles, false, INFINITE);
  if (handles[res - WAIT_OBJECT_0] == gExitEvent) {
    CancelWaitableTimer(p->TimerEvent);
    return false;
  }
  return true;
}

}// namespace OpenKneeboard
