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
