#pragma once

// Needed before detours.h for 'portability' defines
#include <Windows.h>
#include <detours.h>

void DetourUpdateAllThreads();
void DetourTransactionPushBegin();
void DetourTransactionPopCommit();

/** "C++ does not support casting a member function pointer to a void*"
 *
 *     ^...^   m1a
 *    / o,o \
 *    |):::(|
 *  ====w=w===
 *
 *    O RLY?
 */
template <typename T>
void* sudo_make_me_a_void_pointer(T any) {
  union {
    T any;
    void* pVoid;
  } u;
  u.any = any;
  return u.pVoid;
}
