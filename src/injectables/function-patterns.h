#pragma once
#include <string>
#include <vector>

namespace OpenKneeboard {

/** Convert a pattern containing bytes or `?` into a bit pattern and
 * bit mask */
std::vector<std::pair<uint64_t, uint64_t>> ComputeFunctionPatterns(
  const std::basic_string_view<unsigned char>& rawPattern);

/** Search for a pattern on a 16-byte-aligned offset in the specified
 * memory range. */
void* FindFunctionPattern(
  const std::vector<std::pair<uint64_t, uint64_t>>& allPatterns,
  void* _begin,
  void* _end);

/** Search for a pattern in a 16-byte-aligned offset in the specified
 * module/DLL. */
void* FindFunctionPatternInModule(
  const char* moduleName,
  const std::basic_string_view<unsigned char>& rawPattern,
  bool* foundMultiple);

}// namespace OpenKneeboard
