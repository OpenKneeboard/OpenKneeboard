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
#pragma once

// Needed before detours.h for 'portability' defines
#include <Windows.h>
#include <detours.h>

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

/// Create a transaction, detach a single detour, and submit the transaction
LONG DetourSingleDetach(void** ppPointer, void* pDetour);

template <detours_function_pointer T>
LONG DetourSingleDetach(T* ppPointer, T pDetour) {
  return DetourSingleDetach(
    reinterpret_cast<void**>(ppPointer), reinterpret_cast<void*>(pDetour));
}
