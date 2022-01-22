#include "function-patterns.h"

#include <OpenKneeboard/dprint.h>

// @clang-format off
#include <Windows.h>
#include <Psapi.h>
// @clang-format on

namespace OpenKneeboard {

std::vector<std::pair<uint64_t, uint64_t>> ComputeFunctionPatterns(
  const std::basic_string_view<unsigned char>& rawPattern) {
  std::vector<std::pair<uint64_t, uint64_t>> patterns;
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
      FMT_STRING("{:016x} (mask {:016x})"),
      // TODO (C++23) std::byteswap
      _byteswap_uint64(pattern.first),
      _byteswap_uint64(pattern.second));
  }

  return patterns;
}

void* FindFunctionPattern(
  const std::vector<std::pair<uint64_t, uint64_t>>& allPatterns,
  void* _begin,
  void* _end) {
  auto begin = reinterpret_cast<uint64_t*>(_begin);
  auto end = reinterpret_cast<uint64_t*>(_end);
  dprintf(
    FMT_STRING("Code search range: {:#018x}-{:#018x}"),
    (uint64_t)begin,
    (uint64_t)end);

  const uint64_t firstPattern = allPatterns.front().first,
                 firstMask = allPatterns.front().second;
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

      if ((*it & pattern.second) != pattern.first) {
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
  const std::basic_string_view<unsigned char>& rawPattern,
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
    + (pattern.size() * sizeof(pattern.front().first));
  // 16-byte alignment for all stack addresses
  nextAddr -= (nextAddr % 16);
  begin = reinterpret_cast<void*>(nextAddr);
  if (FindFunctionPattern(pattern, begin, end)) {
    *foundMultiple = true;
  }

  return addr;
}

}// namespace OpenKneeboard
