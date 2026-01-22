// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <shims/vulkan/vulkan.h>

#include <concepts>
#include <string>
#include <unordered_set>
#include <vector>

namespace OpenKneeboard::Vulkan {

// clang-format off
template <class T>
concept vk_create_info_with_extensions =
  requires (T t) {
    t.sType;
    t.pNext;
    t.enabledExtensionCount;
    t.ppEnabledExtensionNames;
  } && std::same_as<
    VkStructureType,
    decltype(std::declval<T>().sType)
  > && std::same_as<
    const void*,
    decltype(std::declval<T>().pNext)
  > && std::same_as<
    uint32_t,
    decltype(std::declval<T>().enabledExtensionCount)
  > && std::same_as<
    const char* const*,
    decltype(std::declval<T>().ppEnabledExtensionNames)
  >;
// clang-format on

static_assert(vk_create_info_with_extensions<VkInstanceCreateInfo>);
static_assert(vk_create_info_with_extensions<VkDeviceCreateInfo>);

template <vk_create_info_with_extensions T>
struct ExtendedCreateInfo : public T {
 public:
  using VkCreateInfo = T;
  ExtendedCreateInfo() = delete;

  ExtendedCreateInfo(const T& base, const auto& requiredExtensions) {
    static_cast<T&>(*this) = base;

    std::unordered_set<std::string_view> extensions;

    mExtensionNameStrings.reserve(base.enabledExtensionCount);
    for (size_t i = 0; i < base.enabledExtensionCount; ++i) {
      mExtensionNameStrings.push_back(base.ppEnabledExtensionNames[i]);
      extensions.emplace(mExtensionNameStrings.back());
    }

    for (const auto& ext: requiredExtensions) {
      if (extensions.contains(ext)) {
        continue;
      }

      mExtensionNameStrings.push_back(std::string {ext});
      extensions.emplace(mExtensionNameStrings.back());
    }

    mExtensionNamePointers.reserve(mExtensionNameStrings.size());
    for (const auto& string: mExtensionNameStrings) {
      mExtensionNamePointers.push_back(string.c_str());
    }

    this->enabledExtensionCount = mExtensionNamePointers.size();
    this->ppEnabledExtensionNames = mExtensionNamePointers.data();
  }

 private:
  // We need to keep alive both:
  // - an array of C strings to pass to VK
  // - the strings that that array points to
  std::vector<std::string> mExtensionNameStrings;
  std::vector<const char*> mExtensionNamePointers;
};

template <
  vk_create_info_with_extensions TFirst,
  std::derived_from<typename TFirst::VkCreateInfo>... TRest>
struct CombinedCreateInfo;

template <
  vk_create_info_with_extensions TFirst,
  std::derived_from<typename TFirst::VkCreateInfo> TSecond,
  std::derived_from<typename TFirst::VkCreateInfo>... TRest>
struct CombinedCreateInfo<TFirst, TSecond, TRest...>
  : public TFirst::VkCreateInfo {
 public:
  using VkCreateInfo = typename TFirst::VkCreateInfo;
  CombinedCreateInfo(const VkCreateInfo& base) : mFirst(base), mRest(mFirst) {
    memcpy(this, &mRest, sizeof(VkCreateInfo));
  }

 private:
  const TFirst mFirst;
  const CombinedCreateInfo<TSecond, TRest...> mRest;
};

template <vk_create_info_with_extensions TFirst>
struct CombinedCreateInfo<TFirst> : public TFirst {};

}// namespace OpenKneeboard::Vulkan
