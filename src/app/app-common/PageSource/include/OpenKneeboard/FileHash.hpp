// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace OpenKneeboard {

// FNV-1a 64-bit hash over the first 64 KiB of a file plus its total size.
// Not a cryptographic hash; used only to detect replaced files so stale
// bookmarks are not silently applied to changed content.
inline uint64_t ComputeFileHash(const std::filesystem::path& path) noexcept {
  try {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
      return 0;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      return 0;
    }
    const auto readSize = static_cast<std::streamsize>(
      std::min<std::uintmax_t>(size, 65536));
    std::vector<char> buf(readSize);
    file.read(buf.data(), readSize);
    const auto n = static_cast<std::size_t>(file.gcount());

    constexpr uint64_t kFNVOffsetBasis = 14695981039346656037ULL;
    constexpr uint64_t kFNVPrime = 1099511628211ULL;
    uint64_t hash = kFNVOffsetBasis;
    for (std::size_t i = 0; i < n; ++i) {
      hash ^= static_cast<uint8_t>(buf[i]);
      hash *= kFNVPrime;
    }
    // Also mix in the file size to distinguish files with identical prefixes.
    const auto* sizeBytes = reinterpret_cast<const uint8_t*>(&size);
    for (std::size_t i = 0; i < sizeof(size); ++i) {
      hash ^= sizeBytes[i];
      hash *= kFNVPrime;
    }
    return hash ? hash : 1;
  } catch (...) {
    return 0;
  }
}

}// namespace OpenKneeboard
