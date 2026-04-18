// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <system_error>

namespace OpenKneeboard {

// FNV-1a 64-bit hash over the first 64 KiB of a file plus its total size.
// Not a cryptographic hash; used only to detect replaced files so stale
// bookmarks are not silently applied to changed content.
std::expected<uint64_t, std::error_code> PartialFileHash(
  const std::filesystem::path& path) noexcept;

}// namespace OpenKneeboard
