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
template <class TOut, class TIn>
void* sudo_make_me_a(TIn in) {
  union {
    TIn in;
    TOut out;
  } u;
  u.in = in;
  return u.out;
}
