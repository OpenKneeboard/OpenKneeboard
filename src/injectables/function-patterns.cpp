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
#include "function-patterns.h"

#include <OpenKneeboard/dprint.h>

// clang-format off
#include <Windows.h>
#include <Psapi.h>
// clang-format on

namespace OpenKneeboard {

std::vector<BytePattern> ComputeFunctionPatterns(
  std::basic_string_view<unsigned char> rawPattern) {
  std::vector<BytePattern> patterns;
  auto view(rawPattern);
  while (!view.empty()) {
    std::string pattern(8, '\0');
    std::string mask(8, '\0');
    for (auto i = 0; i < 8 && i < view.size(); ++i) {
      if (view[i] != '?') {
        pattern[i] = view[i];
        mask[i] = 0xffi8;
      }
    }
    patterns.push_back(
      {*(uint64_t*)(pattern.data()), *(uint64_t*)(mask.data())});

    if (view.size() <= 8) {
      break;
    }
    view = view.substr(8);
  }

  dprint("Code search pattern:");
  for (auto& pattern: patterns) {
    dprintf(
      "{:016x} (mask {:016x})",
      std::byteswap(pattern.value),
      std::byteswap(pattern.mask));
  }

  return patterns;
}

void* FindFunctionPattern(
  const std::vector<BytePattern>& allPatterns,
  void* _begin,
  void* _end) {
  auto begin = reinterpret_cast<uint64_t*>(_begin);
  auto end = reinterpret_cast<uint64_t*>(_end);
  dprintf(
    "Code search range: {:#018x}-{:#018x}", (uint64_t)begin, (uint64_t)end);

  const uint64_t firstPattern = allPatterns.front().value,
                 firstMask = allPatterns.front().mask;
  auto patterns = allPatterns;
  patterns.erase(patterns.begin());
  // Stack entries (including functions) are always aligned on 16-byte
  // boundaries
  for (auto func = begin; func < end; func += (16 / sizeof(*func))) {
    auto it = func;
    if ((*it & firstMask) != firstPattern) {
      continue;
    }
    for (auto& pattern: patterns) {
      it++;
      if (it >= end) {
        return nullptr;
      }

      if ((*it & pattern.mask) != pattern.value) {
        goto FindFunctionPattern_NextBlock;
      }
    }
    return reinterpret_cast<void*>(func);

  FindFunctionPattern_NextBlock:
    continue;
  }

  return nullptr;
}

void* FindFunctionPatternInModule(
  const char* moduleName,
  std::basic_string_view<unsigned char> rawPattern,
  bool* foundMultiple) {
  auto hModule = GetModuleHandleA(moduleName);
  if (!hModule) {
    dprintf("Module {} is not loaded.", moduleName);
    return nullptr;
  }
  MODULEINFO info;
  if (!GetModuleInformation(
        GetCurrentProcess(), hModule, &info, sizeof(info))) {
    dprintf("Failed to GetModuleInformation() for {}", moduleName);
    return 0;
  }

  auto begin = info.lpBaseOfDll;
  auto end = reinterpret_cast<void*>(
    reinterpret_cast<std::byte*>(info.lpBaseOfDll) + info.SizeOfImage);
  auto pattern = ComputeFunctionPatterns(rawPattern);
  auto addr = FindFunctionPattern(pattern, begin, end);
  if (addr == nullptr || foundMultiple == nullptr) {
    return addr;
  }

  auto nextAddr = reinterpret_cast<uintptr_t>(addr)
    + (pattern.size() * sizeof(pattern.front().value));
  // 16-byte alignment for all stack addresses
  nextAddr -= (nextAddr % 16);
  begin = reinterpret_cast<void*>(nextAddr);
  if (FindFunctionPattern(pattern, begin, end)) {
    *foundMultiple = true;
  }

  return addr;
}

}// namespace OpenKneeboard
