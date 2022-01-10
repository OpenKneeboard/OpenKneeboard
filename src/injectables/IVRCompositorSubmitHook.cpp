#include "IVRCompositorSubmitHook.h"

#include <OpenKneeboard/dprint.h>

#include <stdexcept>

#include "detours-ext.h"

namespace OpenKneeboard {

namespace {

struct IVRCompositor_VTable {
  void* mSetTrackingSpace;
  void* mGetTrackingSpace;
  void* mWaitGetPoses;
  void* mGetLastPoses;
  void* mGetLastPoseForTrackedDeviceIndex;
  void* mSubmit;
};

IVRCompositorSubmitHook* gHook = nullptr;
decltype(&vr::IVRCompositor::Submit) Real_IVRCompositor_Submit = nullptr;

template <typename T>
void* member_function_to_void_pointer_cast(T mfp) {
  union {
    T pT;
    void* pVoid;
  } u;
  u.pT = mfp;
  return u.pVoid;
}

class ScopedRWX {
  MEMORY_BASIC_INFORMATION mMBI;
  DWORD mOldProtection;
  public:
    ScopedRWX(void* addr) {
      VirtualQuery(addr, &mMBI, sizeof(mMBI));
      VirtualProtect(mMBI.BaseAddress, mMBI.RegionSize, PAGE_EXECUTE_READWRITE, &mOldProtection);
    }

    ~ScopedRWX() {
      DWORD rwx;
      VirtualProtect(mMBI.BaseAddress, mMBI.RegionSize, mOldProtection, &rwx);
    }
};

}// namespace

struct IVRCompositorSubmitHook::Impl {
  IVRCompositor_VTable* mVTable = nullptr;

  vr::EVRCompositorError Hooked_IVRCompositor_Submit(
    vr::EVREye eEye,
    const vr::Texture_t* pTexture,
    const vr::VRTextureBounds_t* pBounds,
    vr::EVRSubmitFlags nSubmitFlags) {
    dprintf("{}", __FUNCTION__);
    auto this_ = reinterpret_cast<vr::IVRCompositor*>(this);
    if (!gHook) {
      return std::invoke(
        Real_IVRCompositor_Submit,
        this_,
        eEye,
        pTexture,
        pBounds,
        nSubmitFlags);
    }

    return gHook->OnIVRCompositor_Submit(
      eEye, pTexture, pBounds, nSubmitFlags, this_, Real_IVRCompositor_Submit);
  }
};

IVRCompositorSubmitHook::IVRCompositorSubmitHook()
  : p(std::make_unique<Impl>()) {
  if (gHook) {
    throw std::logic_error("Can only have one IVRCompositorSubmitHook");
  }

  gHook = this;

  auto VR_GetGenericInterface
    = reinterpret_cast<decltype(&vr::VR_GetGenericInterface)>(
      DetourFindFunction("openvr_api.dll", "VR_GetGenericInterface"));
  if (VR_GetGenericInterface) {
    dprintf("Found OpenVR API");
  } else {
    dprintf("Did not find OpenVR API");
    return;
  }

  vr::EVRInitError vrError;
  auto compositor = reinterpret_cast<vr::IVRCompositor*>(
    VR_GetGenericInterface(vr::IVRCompositor_Version, &vrError));
  if (!compositor) {
    // TODO: hook VR_GetGenericInterface if we don't already have a compositor
    // in case this DLL was attached too early
    dprintf("No OpenVR compositor found: {}", static_cast<int>(vrError));
    return;
  }

  dprintf("Got an OpenVR compositor");
  p->mVTable = *reinterpret_cast<IVRCompositor_VTable**>(compositor);
  *reinterpret_cast<void**>(&Real_IVRCompositor_Submit) = p->mVTable->mSubmit;

  dprintf("Found Submit at: {:#10x}", (uint64_t)p->mVTable->mSubmit);

  // Just using Detours for locking
  DetourTransactionPushBegin();
  {
    ScopedRWX rwx(p->mVTable);
    p->mVTable->mSubmit
      = member_function_to_void_pointer_cast(&Impl::Hooked_IVRCompositor_Submit);
  }
  DetourTransactionPopCommit();
}

IVRCompositorSubmitHook::~IVRCompositorSubmitHook() {
  Unhook();

  if (gHook == this) {
    gHook = nullptr;
  }
}

void IVRCompositorSubmitHook::Unhook() {
  if (!p->mVTable) {
    return;
  }
  DetourTransactionPushBegin();
  {
    ScopedRWX rwx(p->mVTable);
    p->mVTable->mSubmit = member_function_to_void_pointer_cast(Real_IVRCompositor_Submit);
    p->mVTable = nullptr;
  }
  DetourTransactionPopCommit();

  gHook = nullptr;
}

bool IVRCompositorSubmitHook::IsHookInstalled() const {
  return false;
}

}// namespace OpenKneeboard
