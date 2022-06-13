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

#include <openxr/openxr.h>

#include <utility>

#define OPENKNEEBOARD_NEXT_OPENXR_FUNCS \
  IT(xrDestroyInstance) \
  IT(xrCreateSession) \
  IT(xrDestroySession) \
  IT(xrEndFrame) \
  IT(xrCreateSwapchain) \
  IT(xrEnumerateSwapchainImages) \
  IT(xrAcquireSwapchainImage) \
  IT(xrReleaseSwapchainImage) \
  IT(xrWaitSwapchainImage) \
  IT(xrDestroySwapchain) \
  IT(xrCreateReferenceSpace) \
  IT(xrDestroySpace) \
  IT(xrLocateSpace)

namespace OpenKneeboard {

class OpenXRNext final {
 public:
  OpenXRNext(XrInstance, PFN_xrGetInstanceProcAddr);

#define IT(func) \
  template <class... Args> \
  auto func(Args&&... args) { \
    return this->m_##func(std::forward<Args>(args)...); \
  }
  IT(xrGetInstanceProcAddr)
  OPENKNEEBOARD_NEXT_OPENXR_FUNCS
#undef IT

 private:
#define IT(func) PFN_##func m_##func {nullptr};
  IT(xrGetInstanceProcAddr)
  OPENKNEEBOARD_NEXT_OPENXR_FUNCS
#undef IT
};

}// namespace OpenKneeboard
