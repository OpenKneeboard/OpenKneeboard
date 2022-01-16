#include "detours-ext.h"

#include <OpenKneeboard/dprint.h>

#include <mutex>
#include <vector>

using namespace OpenKneeboard;

namespace {

uint16_t gDepth = 0;
std::recursive_mutex gThreadLock;
}// namespace

DetourTransaction::DetourTransaction() {
  std::scoped_lock guard(gThreadLock);
  if (gDepth++ > 0) {
    return;
  }

  // Keep the lock until we free it in the destructor
  gThreadLock.lock();
  DetourTransactionBegin();
  // We intentionally do not call `DetourUpdateThread()`:
  // - it uses new/delete so can deadlock if a thread is
  //   doing any of a large class of memory operations
  //   microsoft/detours#70
  // - *all* new/deletes/mallocs will fail in any thread
  //   if we have frozen a thread doing something similar
  //
  // It's much more reliable for us to take the risk that
  // a thread RIP is in the first 5 bytes of a function
  // we're hooking.
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
  gThreadLock.unlock();
}
