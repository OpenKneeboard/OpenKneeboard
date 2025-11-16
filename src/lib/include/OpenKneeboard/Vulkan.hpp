// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Vulkan/Dispatch.hpp>
#include <OpenKneeboard/Vulkan/SpriteBatch.hpp>
#include <OpenKneeboard/Vulkan/vkresult.hpp>

#include <shims/vulkan/vulkan.h>

#include <OpenKneeboard/dprint.hpp>

namespace OpenKneeboard::Vulkan {

std::optional<uint32_t> FindMemoryType(
  Dispatch*,
  VkPhysicalDevice,
  uint32_t filter,
  VkMemoryPropertyFlags flags);
}// namespace OpenKneeboard::Vulkan