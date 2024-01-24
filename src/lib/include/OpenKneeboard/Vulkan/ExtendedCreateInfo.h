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
#pragma once

#include <vulkan/vulkan.h>

#include <concepts>
#include <string>
#include <unordered_set>
#include <vector>

namespace OpenKneeboard::Vulkan {

// clang-format off
template <class T>
concept has_extension_list =
  requires (T t) {
    t.enabledExtensionCount;
    t.ppEnabledExtensionNames;
  } && std::same_as<
    uint32_t,
    decltype(std::declval<T>().enabledExtensionCount)
  > && std::same_as<
    const char* const*,
    decltype(std::declval<T>().ppEnabledExtensionNames)
  >;
// clang-format on

static_assert(has_extension_list<VkInstanceCreateInfo>);
static_assert(has_extension_list<VkDeviceCreateInfo>);

template <has_extension_list T>
struct ExtendedCreateInfo : public T {
 public:
  ExtendedCreateInfo() = delete;

  ExtendedCreateInfo(const T& base, const auto& requiredExtensions) {
    static_cast<T&>(*this) = base;

    std::unordered_set<std::string_view> extensions;

    mStrings.reserve(base.enabledExtensionCount);
    for (size_t i = 0; i < base.enabledExtensionCount; ++i) {
      mStrings.push_back(base.ppEnabledExtensionNames[i]);
      extensions.emplace(mStrings.back());
    }

    for (const auto& ext: requiredExtensions) {
      if (extensions.contains(ext)) {
        continue;
      }

      mStrings.push_back(std::string {ext});
      extensions.emplace(mStrings.back());
    }

    mPointers.reserve(mStrings.size());
    for (const auto& string: mStrings) {
      mPointers.push_back(string.c_str());
    }

    this->enabledExtensionCount = mPointers.size();
    this->ppEnabledExtensionNames = mPointers.data();
  }

 private:
  std::vector<std::string> mStrings;
  std::vector<const char*> mPointers;
};

}