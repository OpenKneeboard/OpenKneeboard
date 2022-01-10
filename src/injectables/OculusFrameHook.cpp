#include "OculusFrameHook.h"

#include "detours-ext.h"

// Not declared in modern Oculus SDK headers, but used by some games (like DCS
// World)
OVR_PUBLIC_FUNCTION(ovrResult)
ovr_SubmitFrame2(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount);

#define HOOKED_ENDFRAME_FUNCS \
  IT(ovr_EndFrame) \
  IT(ovr_SubmitFrame) \
  IT(ovr_SubmitFrame2)

namespace OpenKneeboard {

static OculusFrameHook* gInstance = nullptr;

#define IT(x) \
  static_assert(std::same_as<decltype(x), decltype(ovr_EndFrame)>); \
  static decltype(&x) real_##x = nullptr; \
  OVR_PUBLIC_FUNCTION(ovrResult) \
  hooked_##x( \
    ovrSession session, \
    long long frameIndex, \
    const ovrViewScaleDesc* viewScaleDesc, \
    ovrLayerHeader const* const* layerPtrList, \
    unsigned int layerCount) { \
    if (!gInstance) { \
      real_##x(session, frameIndex, viewScaleDesc, layerPtrList, layerCount); \
    } \
    return gInstance->OnOVREndFrame( \
      session, frameIndex, viewScaleDesc, layerPtrList, layerCount, real_##x); \
  }
HOOKED_ENDFRAME_FUNCS
#undef IT

OculusFrameHook::OculusFrameHook() {
  gInstance = this;
  const char* lib = "LibOVRRT64_1.dll";

  DetourTransactionPushBegin();
#define IT(x) \
  real_##x = reinterpret_cast<decltype(&x)>(DetourFindFunction(lib, #x)); \
  DetourAttach(reinterpret_cast<void**>(&real_##x), hooked_##x);
  HOOKED_ENDFRAME_FUNCS
#undef IT
  DetourTransactionPopCommit();
  mHooked = true;
}

void OculusFrameHook::Unhook() {
  if (!mHooked) {
    return;
  }
  mHooked = false;
  DetourTransactionPushBegin();
#define IT(x) DetourDetach(reinterpret_cast<void**>(&real_##x), hooked_##x);
  HOOKED_ENDFRAME_FUNCS
#undef IT
  DetourTransactionPopCommit();
}

OculusFrameHook::~OculusFrameHook() {
  Unhook();
  gInstance = nullptr;
}

}// namespace OpenKneeboard
