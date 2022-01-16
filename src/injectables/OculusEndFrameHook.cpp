#include "OculusEndFrameHook.h"

#include <OpenKneeboard/dprint.h>

#include <stdexcept>

#include "DllHook.h"
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

static const char* MODULE_NAME = "LibOVRRT64_1.dll";

struct OculusEndFrameHook::Impl : public DllHook {
  Impl(OculusEndFrameHook*);
  ~Impl();

  using DllHook::InitWithVTable;

  virtual void InstallHook() override;

 private:
  static OculusEndFrameHook* gHook;
  OculusEndFrameHook* mHook = nullptr;

#define IT(x) \
  static_assert(std::same_as<decltype(x), decltype(ovr_EndFrame)>); \
  static decltype(&x) real_##x; \
  static ovrResult __cdecl x##_hook( \
    ovrSession session, \
    long long frameIndex, \
    const ovrViewScaleDesc* viewScaleDesc, \
    ovrLayerHeader const* const* layerPtrList, \
    unsigned int layerCount) { \
    if (!gHook) { \
      real_##x(session, frameIndex, viewScaleDesc, layerPtrList, layerCount); \
    } \
    return gHook->OnOVREndFrame( \
      session, frameIndex, viewScaleDesc, layerPtrList, layerCount, real_##x); \
  } \
  static_assert(std::same_as<decltype(&x), decltype(&x##_hook)>);
  HOOKED_ENDFRAME_FUNCS
#undef IT
};

OculusEndFrameHook* OculusEndFrameHook::Impl::gHook = nullptr;
#define IT(x) decltype(&x) OculusEndFrameHook::Impl::real_##x = nullptr;
HOOKED_ENDFRAME_FUNCS
#undef IT

OculusEndFrameHook::OculusEndFrameHook() : p(std::make_unique<Impl>(this)) {
  dprintf("{} {:#018x}", __FUNCTION__, (uint64_t)this);
}

OculusEndFrameHook::~OculusEndFrameHook() {
  // Must be called before any other stuff gets deallocated
  this->UninstallHook();
}

void OculusEndFrameHook::InitWithVTable() {
  p->InitWithVTable();
}

void OculusEndFrameHook::UninstallHook() {
  p.reset();
}

void OculusEndFrameHook::OnOVREndFrameHookInstalled() {
}

OculusEndFrameHook::Impl::Impl(OculusEndFrameHook* hook)
  : DllHook(MODULE_NAME), mHook(hook) {
}

void OculusEndFrameHook::Impl::InstallHook() {
  if (gHook != nullptr) {
    throw std::logic_error("Can only have one OculusEndFrameHook at a time");
  }
  gHook = mHook;

  // Find outside of the transaction as DetourFindFunction calls LoadLibrary
#define IT(x) \
  real_##x \
    = reinterpret_cast<decltype(&x)>(DetourFindFunction(MODULE_NAME, #x));
  HOOKED_ENDFRAME_FUNCS
#undef IT

  {
    DetourTransaction dt;
#define IT(x) DetourAttach(reinterpret_cast<void**>(&real_##x), x##_hook);
    HOOKED_ENDFRAME_FUNCS
#undef IT
  }
  dprint("Attached OculusEndFrameHook");

  mHook->OnOVREndFrameHookInstalled();
}

OculusEndFrameHook::Impl::~Impl() {
  if (gHook != mHook) {
    return;
  }

  {
    DetourTransaction dt;
#define IT(x) DetourDetach(reinterpret_cast<void**>(&real_##x), x##_hook);
    HOOKED_ENDFRAME_FUNCS
#undef IT
  }
  gHook = nullptr;
  dprint("Detached OculusEndFrameHook");
}

}// namespace OpenKneeboard
