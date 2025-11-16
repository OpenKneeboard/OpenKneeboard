// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include "OVRProxy.hpp"

#include "OVRRuntimeDLLNames.hpp"
#include "detours-ext.hpp"

#include <OpenKneeboard/dprint.hpp>

#include <mutex>
#include <stdexcept>

namespace OpenKneeboard {

std::shared_ptr<OVRProxy> OVRProxy::gInstance;

std::shared_ptr<OVRProxy> OVRProxy::Get() {
  if (!gInstance) [[unlikely]] {
    gInstance.reset(new OVRProxy());
  }

  return gInstance;
}

static const char* GetActiveRuntimeDLLName() {
  static std::once_flag sOnce;
  static const char* sActiveRuntime {nullptr};
  std::call_once(sOnce, [&runtime = sActiveRuntime]() {
    for (const auto it: OVRRuntimeDLLNames) {
      if (GetModuleHandleA(it)) {
        dprint("OVRProxy found runtime: {}", it);
        runtime = it;
        return;
      }
    }
  });
  return sActiveRuntime;
}

#define X(x) \
  x(reinterpret_cast<decltype(x)>( \
    DetourFindFunction(GetActiveRuntimeDLLName(), #x))),
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
