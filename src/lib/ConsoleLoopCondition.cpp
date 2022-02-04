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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <OpenKneeboard/ConsoleLoopCondition.h>
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
  HANDLE mTimerEvent = nullptr;
};

ConsoleLoopCondition::ConsoleLoopCondition() : p(std::make_shared<Impl>()) {
  gExitEvent = CreateEvent(nullptr, false, false, nullptr);
  p->mTimerEvent = CreateWaitableTimer(nullptr, true, nullptr);
  SetConsoleCtrlHandler(&ExitHandler, true);
}

ConsoleLoopCondition::~ConsoleLoopCondition() {
  SetConsoleCtrlHandler(&ExitHandler, false);
  CloseHandle(p->mTimerEvent);
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
    p->mTimerEvent, (LARGE_INTEGER*)&expiration, 0, nullptr, nullptr, false);

  HANDLE handles[] = {gExitEvent, p->mTimerEvent};
  auto res = WaitForMultipleObjects(
    sizeof(handles) / sizeof(handles[0]), handles, false, INFINITE);
  if (handles[res - WAIT_OBJECT_0] == gExitEvent) {
    CancelWaitableTimer(p->mTimerEvent);
    return false;
  }
  return true;
}

}// namespace OpenKneeboard
