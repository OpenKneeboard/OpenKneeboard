// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// Needed before detours.h for 'portability' defines
#include <Windows.h>

#include <detours/detours.h>

#include <memory>

/** READ SAFETY WARNING - Suspend all other threads, and patch RIP.
 *
 * WARNING: while a `DetourTransaction` exists, pretty much any use of the heap
 * can (and often will) deadlock.
 *
 * This includes:
 * - new/delete/malloc/free
 * - on-stack objects with heap-allocated content (e.g. `std::vector`) going
 *   out of scope
 * - `printf`, `std::format`, `dprint` etc
 */
class DetourTransaction final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  DetourTransaction();
  ~DetourTransaction() noexcept;
};

template <class T>
concept detours_function_pointer = DetoursIsFunctionPointer<T>::value;

/// Create a transaction, attach a single detour, and submit the transaction
LONG DetourSingleAttach(void** ppPointer, void* pDetour);

template <detours_function_pointer T>
LONG DetourSingleAttach(T* ppPointer, T pDetour) {
  return DetourSingleAttach(
    reinterpret_cast<void**>(ppPointer), reinterpret_cast<void*>(pDetour));
}

// Required for VS 2022 17.6 (1936) and below, compile error for 17.7 (1937) and
// above
#if _MSC_VER < 1937 && !defined(CLANG_TIDY) && !defined(CLANG_CL)
template <detours_function_pointer T>
LONG DetourAttach(T* ppPointer, T pDetour) {
  return DetourAttach(reinterpret_cast<void**>(ppPointer), pDetour);
}
#endif

/// Create a transaction, detach a single detour, and submit the transaction
LONG DetourSingleDetach(void** ppPointer, void* pDetour);

template <detours_function_pointer T>
LONG DetourSingleDetach(T* ppPointer, T pDetour) {
  return DetourSingleDetach(
    reinterpret_cast<void**>(ppPointer), reinterpret_cast<void*>(pDetour));
}
