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

#include <OpenKneeboard/ThreadGuard.h>

#include <OpenKneeboard/dprint.h>

namespace OpenKneeboard {

ThreadGuard::ThreadGuard(const std::source_location& loc) : mLocation(loc) {
#ifdef DEBUG
  mThreadID = GetCurrentThreadId();
#endif
}

void ThreadGuard::CheckThread() const {
#ifdef DEBUG
  const auto thisThread = GetCurrentThreadId();
  if (thisThread == mThreadID) {
    return;
  }
  dprintf(
    "ThreadGuard mismatch: was {} ({:#x}), now {} ({:#x})",
    mThreadID,
    mThreadID,
    thisThread,
    thisThread);
  dprintf("Created at {}", mLocation);
  OPENKNEEBOARD_BREAK;
#endif
}

ThreadGuard::~ThreadGuard() {
  this->CheckThread();
}

}// namespace OpenKneeboard