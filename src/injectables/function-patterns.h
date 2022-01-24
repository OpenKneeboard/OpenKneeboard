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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
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
