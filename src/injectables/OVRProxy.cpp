#include "OVRProxy.h"

#include "detours-ext.h"

namespace OpenKneeboard {

#define X(x) \
  x(reinterpret_cast<decltype(x)>(DetourFindFunction("LibOVRRT64_1.dll", #x))),
OVRProxy::OVRProxy()
  : OPENKNEEBOARD_OVRPROXY_FUNCS
#undef X
    // We need something after the trailing comma...
    mNullptr(nullptr) {
}

}// namespace OpenKneeboard
