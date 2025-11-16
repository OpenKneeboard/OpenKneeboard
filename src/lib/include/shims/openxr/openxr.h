// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/config.hpp>

#ifdef OPENKNEEBOARD_32BIT_BUILD

#include <OpenKneeboard/Opaque64BitHandle.hpp>

namespace OpenKneeboard::OpenXR::Detail {

/*
 * The OpenXR headers define handles by default in a way such that
 * all handles in 32-bit builds are considered the same type, so can't be used
 * in overload resolution. Let's fix this.
 */
template <class T>
struct Handle64 : public OpenKneeboard::Opaque64BitHandle<T> {
  using Opaque64BitHandle<T>::Opaque64BitHandle;
};

}// namespace OpenKneeboard::OpenXR::Detail

#define XR_DEFINE_HANDLE(object) \
  struct object : public ::OpenKneeboard::OpenXR::Detail::Handle64<object> { \
    using Handle64<object>::Handle64; \
  };
// As Vulkan usually uses `uint64_t` for 32-bit handles, it can't use nullptr;
// as we have a nullptr overload in our struct, it'll work fine
#define XR_NULL_HANDLE nullptr
#endif// OPENKNEEBOARD_32BIT_BUILD

#include <openxr/openxr.h>