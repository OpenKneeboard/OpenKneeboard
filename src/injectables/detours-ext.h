#pragma once

// Needed before detours.h for 'portability' defines
#include <Windows.h>
#include <detours.h>

#include <memory>

/** Suspend all other threads, and patch RIP.
 *
 * It is unsafe to call new/delete/malloc/free or many simialr functions
 * while an instance of this class exists.
 */
class DetourTransaction final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  DetourTransaction();
  ~DetourTransaction() noexcept;
};

template<class T>
concept detours_function_pointer =
DetoursIsFunctionPointer<T>::value;

/// Create a transaction, attach a single detour, and submit the transaction
LONG DetourSingleAttach(void** ppPointer, void* pDetour);

template<detours_function_pointer T>
LONG DetourSingleAttach(T* ppPointer, T pDetour) {
	return DetourSingleAttach(
		reinterpret_cast<void**>(ppPointer), reinterpret_cast<void*>(pDetour));
}

/// Create a transaction, detach a single detour, and submit the transaction
LONG DetourSingleDetach(void** ppPointer, void* pDetour);

template<detours_function_pointer T>
LONG DetourSingleDetach(T* ppPointer, T pDetour) {
	return DetourSingleDetach(
		reinterpret_cast<void**>(ppPointer), reinterpret_cast<void*>(pDetour));
}
