// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <filesystem>

namespace OpenKneeboard {

bool FilesDiffer(
  const std::filesystem::path& a,
  const std::filesystem::path& b);

}
