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

#ifdef OPENKNEEBOARD_32BIT_BUILD

#include <OpenKneeboard/Opaque64BitHandle.h>

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