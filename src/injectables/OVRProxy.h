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

#include <memory>

#include <OVR_CAPI.h>
#include <OVR_CAPI_D3D.h>

namespace OpenKneeboard {

#define OPENKNEEBOARD_OVRPROXY_FUNCS \
  X(ovr_GetTrackingState) \
  X(ovr_GetPredictedDisplayTime) \
  X(ovr_CreateTextureSwapChainDX) \
  X(ovr_GetTextureSwapChainLength) \
  X(ovr_GetTextureSwapChainBufferDX) \
  X(ovr_GetTextureSwapChainCurrentIndex) \
  X(ovr_CommitTextureSwapChain) \
  X(ovr_DestroyTextureSwapChain)

struct OVRProxy {
  static std::shared_ptr<OVRProxy> Get();
#define X(x) const decltype(&x) x = nullptr;
  OPENKNEEBOARD_OVRPROXY_FUNCS;
#undef X

 private:
  static std::shared_ptr<OVRProxy> gInstance;
  OVRProxy();
  // Implementation detail when we need something after a trailing comma
  // for macro funkiness
  [[maybe_unused]] decltype(nullptr) mNullptr;
};

}// namespace OpenKneeboard
