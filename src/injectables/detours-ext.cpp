#include "detours-ext.h"

#include <OpenKneeboard/dprint.h>
#include <TlHelp32.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace OpenKneeboard;

namespace {

std::vector<HANDLE> GetAllThreads() {
  std::vector<HANDLE> handles;
  auto snapshot
    = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());
  THREADENTRY32 thread;
  thread.dwSize = sizeof(thread);

  DetourUpdateThread(GetCurrentThread());
  auto myProc = GetCurrentProcessId();
  auto myThread = GetCurrentThreadId();
  if (!Thread32First(snapshot, &thread)) {
    dprintf("Failed to find first thread");
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

// We can't use mutexes here as some of the NT utility threads may have been
// frozen by an existing Detours lock
class RecursiveSpinlock final {
 public:
  void Lock();
  void Unlock();

 private:
  std::atomic_flag mFlag;
  uint32_t mDepth = 0;
  DWORD mThreadId = 0;
  bool TryLock();
};

bool RecursiveSpinlock::TryLock() {
  auto thisThread = GetCurrentThreadId();
  if (mFlag.test_and_set()) {
    if (mThreadId != thisThread) {
      return false;
    }
    mDepth++;
    return true;
  }

  mThreadId = thisThread;
  mDepth++;
  return true;
}

void RecursiveSpinlock::Lock() {
  while (!TryLock()) {
    // do not use std::this_thread::yield() as it can attempt to acquire locks
    // that suspended threads may hold
    YieldProcessor();
  }
}

void RecursiveSpinlock::Unlock() {
  if (--mDepth == 0) {
    mThreadId = 0;
    mFlag.clear();
  }
}

class RecursiveSpinlockGuard final {
 private:
  RecursiveSpinlock* mLock = nullptr;

 public:
  RecursiveSpinlockGuard() = default;
  RecursiveSpinlockGuard(const RecursiveSpinlockGuard&) = delete;

  RecursiveSpinlockGuard(RecursiveSpinlock& lock) {
    mLock = &lock;
    mLock->Lock();
  }

  ~RecursiveSpinlockGuard() {
    if (mLock) {
      mLock->Unlock();
    }
  }

  RecursiveSpinlockGuard& operator=(const RecursiveSpinlockGuard&) = delete;
  RecursiveSpinlockGuard& operator=(RecursiveSpinlockGuard&& other) {
    mLock = other.mLock;
    other.mLock = nullptr;
    return *this;
  }
};

uint16_t gTransactionDepth = 0;
uint16_t gDepth = 0;
RecursiveSpinlock gThreadLock;
}// namespace

struct DetourTransaction::Impl {
  RecursiveSpinlockGuard mThreadLock;
  std::vector<HANDLE> mThreads;
};

DetourTransaction::DetourTransaction() : p(std::make_unique<Impl>()) {
  RecursiveSpinlockGuard guard(gThreadLock);
  if (gDepth++ > 0) {
    return;
  }

  p->mThreadLock = std::move(guard);
  DetourTransactionBegin();
  p->mThreads = GetAllThreads();
  for (auto handle: p->mThreads) {
    // The thread may have finished since we got the list of threads; only
    // tell detour to update it if it still exists.
    //
    // Otherwise, detours gets sad on commit: if it's not able to resume
    // every thread, if gives up and stops trying to resume all the others
    if (SuspendThread(handle) != (DWORD) -1) {
      DetourUpdateThread(handle);
    }
  }
  dprint("DetourTransaction++");
}

DetourTransaction::~DetourTransaction() {
  if (--gDepth > 0) {
    return;
  }

  auto err = DetourTransactionCommit();
  if (err) {
    dprintf("Committing detour transaction failed: {}", err);
  }
  dprint("DetourTransaction--");
  for (auto handle: p->mThreads) {
    // If any thread fails to resume (e.g. maybe it finished before Detours
    // was able to suspend it), detours stops trying to resume any others
    ResumeThread(handle);
    CloseHandle(handle);
  }
}
