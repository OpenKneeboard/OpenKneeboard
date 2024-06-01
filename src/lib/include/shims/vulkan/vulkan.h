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

#include <OpenKneeboard/config.h>

#if OPENKNEEBOARD_32BIT_BUILD
/*
 * The vulkan headers define this by default in a way such that all handles
 * in 32-bit builds are considered the same type, so can't be used in overload
 * resolution. Let's fix this.
 */

namespace OpenKneeboard::Vulkan::Detail {
template <class T>
struct NonDispatchableHandle32 {
  uint64_t mRawValue {};

  constexpr NonDispatchableHandle32() = default;
  constexpr NonDispatchableHandle32(nullptr_t) {};
  constexpr explicit NonDispatchableHandle32(uint64_t value)
    : mRawValue(value) {};

  constexpr NonDispatchableHandle32(const NonDispatchableHandle32<T>&)
    = default;
  constexpr NonDispatchableHandle32(NonDispatchableHandle32<T>&&) = default;

  constexpr NonDispatchableHandle32<T>& operator=(
    const NonDispatchableHandle32<T>&)
    = default;

  constexpr NonDispatchableHandle32<T>& operator=(NonDispatchableHandle32<T>&&)
    = default;
  constexpr operator bool() const {
    return mRawValue != 0;
  }
};

}// namespace OpenKneeboard::Vulkan::Detail

template <class T>
  requires std::
    derived_from<T, OpenKneeboard::Vulkan::Detail::NonDispatchableHandle32<T>>
  struct std::hash<T> {
  constexpr size_t operator()(const T& handle) const noexcept {
    return handle.mRawValue;
  }
};

#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) \
  struct object \
    : public ::OpenKneeboard::Vulkan::Detail::NonDispatchableHandle32< \
        object> { \
    using NonDispatchableHandle32<object>::NonDispatchableHandle32; \
  };
// As Vulkan usually uses `uint64_t` for 32-bit handles, it can't use nullptr;
// as we have a nullptr overload in our struct, it'll work fine
#define VK_NULL_HANDLE nullptr
#endif

#include <vulkan/vulkan.h>