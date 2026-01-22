// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/dprint.hpp>

#include <shims/vulkan/vulkan.h>

#include <source_location>

namespace OpenKneeboard::Vulkan {

constexpr bool VK_SUCCEEDED(VkResult code) { return code >= 0; }

constexpr bool VK_FAILED(VkResult code) { return !VK_SUCCEEDED(code); }

inline VkResult check_vkresult(
  VkResult code,
  const std::source_location& loc = std::source_location::current()) {
  if (VK_FAILED(code)) {
    fatal(loc, "Vulkan call failed: {}", static_cast<int>(code));
  }
  return code;
}
}// namespace OpenKneeboard::Vulkan
