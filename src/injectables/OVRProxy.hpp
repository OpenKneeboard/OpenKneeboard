// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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
