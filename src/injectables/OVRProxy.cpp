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
#include "OVRProxy.h"

#include <stdexcept>

#include "detours-ext.h"

namespace OpenKneeboard {

std::shared_ptr<OVRProxy> OVRProxy::gInstance;

std::shared_ptr<OVRProxy> OVRProxy::Get() {
  if (!gInstance) [[unlikely]] {
    gInstance.reset(new OVRProxy());
  }

  return gInstance;
}

#define X(x) \
  x(reinterpret_cast<decltype(x)>(DetourFindFunction("LibOVRRT64_1.dll", #x))),
OVRProxy::OVRProxy()
  : OPENKNEEBOARD_OVRPROXY_FUNCS
#undef X
    // We need something after the trailing comma...
    mNullptr(nullptr) {

  if (!this->ovr_GetTrackingState) {
    throw std::logic_error(
      "Do not call OVRProxy::Get() until the Oculus SDK is loaded");
  }
}

}// namespace OpenKneeboard
