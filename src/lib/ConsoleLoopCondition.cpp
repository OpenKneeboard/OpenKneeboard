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
#include <OpenKneeboard/ConsoleLoopCondition.h>
#include <OpenKneeboard/Win32.h>

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
  gExitEvent = Win32::CreateEventW(nullptr, false, false, nullptr);
  p->mTimerEvent = Win32::CreateWaitableTimerW(nullptr, true, nullptr);
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
