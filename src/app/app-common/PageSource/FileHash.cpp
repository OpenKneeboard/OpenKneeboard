// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/FileHash.hpp>

#include <algorithm>
#include <fstream>
#include <vector>

namespace OpenKneeboard {

std::expected<uint64_t, std::error_code> PartialFileHash(
  const std::filesystem::path& path) noexcept {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    return std::unexpected(ec);
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::unexpected(
      std::make_error_code(std::errc::no_such_file_or_directory));
  }

  constexpr std::uintmax_t kMaxRead = 65536;
  const auto readSize
    = static_cast<std::streamsize>(std::min<std::uintmax_t>(size, kMaxRead));
  std::vector<char> buf(readSize);
  if (readSize > 0) {
    file.read(buf.data(), readSize);
  }
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
  return hash;
}

}// namespace OpenKneeboard
