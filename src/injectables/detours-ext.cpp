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
#include "detours-ext.hpp"

#include <OpenKneeboard/dprint.hpp>

#include <vector>

#include <TlHelp32.h>

using namespace OpenKneeboard;

namespace {

std::vector<HANDLE> GetAllThreads() {
  std::vector<HANDLE> handles;
  auto snapshot
    = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());
  THREADENTRY32 thread;
  thread.dwSize = sizeof(thread);

  auto myProc = GetCurrentProcessId();
  auto myThread = GetCurrentThreadId();
  if (!Thread32First(snapshot, &thread)) {
    dprint("Failed to find first thread");
    return {};
  }

  do {
    if (!thread.th32ThreadID) {
      continue;
    }
    if (thread.th32OwnerProcessID != myProc) {
      // CreateToolhelp32Snapshot takes a process ID, but ignores it
      continue;
    }
    if (thread.th32ThreadID == myThread) {
      continue;
    }

    handles.push_back(
      OpenThread(THREAD_ALL_ACCESS, false, thread.th32ThreadID));
  } while (Thread32Next(snapshot, &thread));

  return handles;
}

}// namespace

struct DetourTransaction::Impl {
  static bool mActive;
  std::vector<HANDLE> mThreads;
  HANDLE mHeap {nullptr};
};
bool DetourTransaction::Impl::mActive = false;

DetourTransaction::DetourTransaction() : p(std::make_unique<Impl>()) {
  dprint("DetourTransaction++");
  // Must be called before we lock the heap
  p->mThreads = GetAllThreads();

  // Make sure no other thread has the heap lock; if it does when we suspend
  // it, we're going to have a bad time, especially due to microsoft/detours#70
  p->mHeap = GetProcessHeap();
  if (Impl::mActive) {
    throw std::logic_error("DetourTransactions can't be nested");
  }
  Impl::mActive = true;
  if (!HeapLock(p->mHeap)) {
    dprintf("Failed to lock heap: {:#x}", GetLastError());
    OPENKNEEBOARD_BREAK;
    return;
  }
  DetourTransactionBegin();
  for (auto it = p->mThreads.begin(); it != p->mThreads.end(); /* nothing */) {
    auto handle = *it;
    // The thread may have finished since we got the list of threads; only
    // tell detour to update it if it still exists.
    //
    // Otherwise, detours gets sad on commit: if it's not able to resume
    // every thread, if gives up and stops trying to resume all the others
    if (SuspendThread(handle) != (DWORD)-1) {
      DetourUpdateThread(handle);
      it++;
      continue;
    }
    it = p->mThreads.erase(it);
  }
}

DetourTransaction::~DetourTransaction() noexcept {
  auto err = DetourTransactionCommit();
  for (auto handle: p->mThreads) {
    // While Detours resumed the threads, there is a 'suspend count', so we
    // need to also call resume
    ResumeThread(handle);
    CloseHandle(handle);
  }
  Impl::mActive = false;
  if (!HeapUnlock(p->mHeap)) {
    dprintf("Failed to unlock heap: {:#x}", GetLastError());
    OPENKNEEBOARD_BREAK;
  }
  // We need to resume the threads before doing *anything* else:
  // dprint/dprintf can malloc/new, which will deadlock
  if (err) {
    dprintf("Committing detour transaction failed: {}", err);
  }
  dprint("DetourTransaction--");
}

LONG DetourSingleAttach(void** ppPointer, void* pDetour) {
  DetourTransaction dt;
  return DetourAttach(ppPointer, pDetour);
}

LONG DetourSingleDetach(void** ppPointer, void* pDetour) {
  DetourTransaction dt;
  return DetourDetach(ppPointer, pDetour);
}
