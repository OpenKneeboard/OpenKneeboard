// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
#include <string>
#include <vector>

namespace OpenKneeboard {

struct BytePattern {
  uint64_t value;
  uint64_t mask;
};

/** Convert a pattern containing bytes or `?` into a bit pattern and
 * bit mask */
std::vector<BytePattern> ComputeFunctionPatterns(
  std::basic_string_view<unsigned char> rawPattern);

/** Search for a pattern on a 16-byte-aligned offset in the specified
 * memory range. */
void* FindFunctionPattern(
  const std::vector<BytePattern>& allPatterns,
  void* _begin,
  void* _end);

/** Search for a pattern in a 16-byte-aligned offset in the specified
 * module/DLL. */
void* FindFunctionPatternInModule(
  const char* moduleName,
  std::basic_string_view<unsigned char> rawPattern,
  bool* foundMultiple);

}// namespace OpenKneeboard
