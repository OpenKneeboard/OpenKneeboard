// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

// Helper macros when joining or stringifying other macros, e.g.:
//
// `FOO##__COUNTER__` becomes `FOO__COUNTER__`
// OPENKNEEBOARD_CONCAT2(FOO, __COUNTER__) might become `FOO1`, for example

#define OPENKNEEBOARD_CONCAT1(x, y) x##y
#define OPENKNEEBOARD_CONCAT2(x, y) OPENKNEEBOARD_CONCAT1(x, y)

#define OPENKNEEBOARD_STRINGIFY1(x) #x
#define OPENKNEEBOARD_STRINGIFY2(x) OPENKNEEBOARD_STRINGIFY1(x)

// Helper for testing __VA_ARG__ behavior
#define OPENKNEEBOARD_THIRD_ARG(a, b, c, ...) c

#define OPENKNEEBOARD_VA_OPT_SUPPORTED_IMPL(...) \
  OPENKNEEBOARD_THIRD_ARG(__VA_OPT__(, ), true, false, __VA_ARGS__)
#define OPENKNEEBOARD_VA_OPT_SUPPORTED OPENKNEEBOARD_VA_OPT_SUPPORTED_IMPL(JUNK)

#define OPENKNEEBOARD_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION_HELPER(X, ...) \
  X##__VA_ARGS__
#define OPENKNEEBOARD_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION \
  OPENKNEEBOARD_THIRD_ARG( \
    OPENKNEEBOARD_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION_HELPER(JUNK), \
    false, \
    true)

#if OPENKNEEBOARD_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION
static_assert(
  OPENKNEEBOARD_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION_HELPER(123) == 123);
#endif