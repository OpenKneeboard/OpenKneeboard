#include "IVRCompositorWaitGetPosesHook.h"

#include <OpenKneeboard/dprint.h>

#include <stdexcept>

#include "DllLoadWatcher.h"
#include "detours-ext.h"

namespace OpenKneeboard {

namespace {

struct IVRCompositor_VTable {
  void* mSetTrackingSpace;
  void* mGetTrackingSpace;
  void* mWaitGetPoses;
};

IVRCompositorWaitGetPosesHook* gHook = nullptr;
decltype(&vr::IVRCompositor::WaitGetPoses) Real_IVRCompositor_WaitGetPoses
  = nullptr;
decltype(&vr::VR_GetGenericInterface) Real_VR_GetGenericInterface = nullptr;

class ScopedRWX {
  MEMORY_BASIC_INFORMATION mMBI;
  DWORD mOldProtection;

 public:
  ScopedRWX(void* addr) {
    VirtualQuery(addr, &mMBI, sizeof(mMBI));
    VirtualProtect(
      mMBI.BaseAddress,
      mMBI.RegionSize,
      PAGE_EXECUTE_READWRITE,
      &mOldProtection);
  }

  ~ScopedRWX() {
    DWORD rwx;
    VirtualProtect(mMBI.BaseAddress, mMBI.RegionSize, mOldProtection, &rwx);
  }
};

}// namespace

struct IVRCompositorWaitGetPosesHook::Impl : public DllLoadWatcher {
  Impl() : DllLoadWatcher("openvr_api.dll") {
  }
  ~Impl() {
    Unhook();
  }

  IVRCompositor_VTable* mVTable = nullptr;

  vr::EVRCompositorError Hooked_IVRCompositor_WaitGetPoses(
    vr::TrackedDevicePose_t* pRenderPoseArray,
    uint32_t unRenderPoseArrayCount,
    vr::TrackedDevicePose_t* pGamePoseArray,
    uint32_t unGamePoseArrayCount);

  void InstallHook();
  void Unhook();

  void InstallCompositorHook(vr::IVRCompositor* compositor);
  void InstallVRGetGenericInterfaceHook();

 protected:
  virtual void OnDllLoad(const std::string& name) override;

 private:
  bool mHookedVRGetGenericInterface = false;
};

IVRCompositorWaitGetPosesHook::IVRCompositorWaitGetPosesHook()
  : p(std::make_unique<Impl>()) {
  dprint(__FUNCTION__);
  if (gHook) {
    dprintf("{}:{}", __FILE__, __LINE__);
    throw std::logic_error("Can only have one IVRCompositorWaitGetPosesHook");
  }

  gHook = this;

  if (!GetModuleHandleA("openvr_api.dll")) {
    dprint("Did not find openvr_api.dll");
    return;
  }

  dprint("Found openvr_api.dll, hooking");
  p->InstallHook();
}

void IVRCompositorWaitGetPosesHook::Impl::InstallCompositorHook(
  vr::IVRCompositor* compositor) {
  dprintf("Got an OpenVR compositor");
  mVTable = *reinterpret_cast<IVRCompositor_VTable**>(compositor);
  *reinterpret_cast<void**>(&Real_IVRCompositor_WaitGetPoses)
    = mVTable->mWaitGetPoses;

  dprintf("Found WaitGetPoses at: {:#018x}", (uint64_t)mVTable->mWaitGetPoses);

  // Just using Detours for locking
  DetourTransactionPushBegin();
  {
    ScopedRWX rwx(mVTable);
    mVTable->mWaitGetPoses
      = sudo_make_me_a_void_pointer(&Impl::Hooked_IVRCompositor_WaitGetPoses);
  }
  DetourTransactionPopCommit();
}

IVRCompositorWaitGetPosesHook::~IVRCompositorWaitGetPosesHook() {
  Unhook();

  if (gHook == this) {
    gHook = nullptr;
  }
}

void IVRCompositorWaitGetPosesHook::Unhook() {
  if (!p->mVTable) {
    return;
  }
  DetourTransactionPushBegin();
  {
    ScopedRWX rwx(p->mVTable);
    p->mVTable->mWaitGetPoses
      = sudo_make_me_a_void_pointer(Real_IVRCompositor_WaitGetPoses);
    p->mVTable = nullptr;
  }
  p->Unhook();
  DetourTransactionPopCommit();

  gHook = nullptr;
}

namespace {

IVRCompositorWaitGetPosesHook::Impl* gHookImpl = nullptr;

void* VR_CALLTYPE Hooked_VR_GetGenericInterface(
  const char* pchInterfaceVersion,
  vr::EVRInitError* peError) {
  auto ret = Real_VR_GetGenericInterface(pchInterfaceVersion, peError);
  if (!gHookImpl) {
    return ret;
  }

  if (std::string_view(pchInterfaceVersion).starts_with("IVRCompositor_")) {
    auto p = gHookImpl;
    p->Unhook();
    p->InstallCompositorHook(reinterpret_cast<vr::IVRCompositor*>(ret));
  }

  return ret;
}

}// namespace

void IVRCompositorWaitGetPosesHook::Impl::InstallVRGetGenericInterfaceHook() {
  mHookedVRGetGenericInterface = true;
  gHookImpl = this;

  DetourTransactionPushBegin();
  DetourAttach(&Real_VR_GetGenericInterface, &Hooked_VR_GetGenericInterface);
  DetourTransactionPopCommit();
}

void IVRCompositorWaitGetPosesHook::Impl::Unhook() {
  if (!mHookedVRGetGenericInterface) {
    return;
  }

  DetourTransactionPushBegin();
  DetourDetach(&Real_VR_GetGenericInterface, &Hooked_VR_GetGenericInterface);
  DetourTransactionPopCommit();

  mHookedVRGetGenericInterface = false;

  if (gHookImpl == this) {
    gHookImpl = nullptr;
  }
}

vr::EVRCompositorError
IVRCompositorWaitGetPosesHook::Impl::Hooked_IVRCompositor_WaitGetPoses(
  vr::TrackedDevicePose_t* pRenderPoseArray,
  uint32_t unRenderPoseArrayCount,
  vr::TrackedDevicePose_t* pGamePoseArray,
  uint32_t unGamePoseArrayCount) {
  auto this_ = reinterpret_cast<vr::IVRCompositor*>(this);
  if (!gHook) {
    return std::invoke(
      Real_IVRCompositor_WaitGetPoses,
      this_,
      pRenderPoseArray,
      unRenderPoseArrayCount,
      pGamePoseArray,
      unGamePoseArrayCount);
  }
  return gHook->OnIVRCompositor_WaitGetPoses(
    this_,
    pRenderPoseArray,
    unRenderPoseArrayCount,
    pGamePoseArray,
    unGamePoseArrayCount,
    Real_IVRCompositor_WaitGetPoses);
}

void IVRCompositorWaitGetPosesHook::Impl::InstallHook() {
  Real_VR_GetGenericInterface
    = reinterpret_cast<decltype(&vr::VR_GetGenericInterface)>(
      DetourFindFunction("openvr_api.dll", "VR_GetGenericInterface"));
  if (Real_VR_GetGenericInterface) {
    dprintf("Found OpenVR API");
  } else {
    dprintf("Did not find OpenVR API");
    return;
  }

  vr::EVRInitError vrError;
  auto compositor = reinterpret_cast<vr::IVRCompositor*>(
    Real_VR_GetGenericInterface(vr::IVRCompositor_Version, &vrError));
  if (!compositor) {
    dprintf("No OpenVR compositor found: {}", static_cast<int>(vrError));
    dprint("Waiting to see if we get one...");
    this->InstallVRGetGenericInterfaceHook();
    return;
  }

  this->InstallCompositorHook(compositor);
}

void IVRCompositorWaitGetPosesHook::Impl::OnDllLoad(const std::string& name) {
  if (name != "openvr_api.dll") {
    return;
  }
  dprint("openvr_api.dll loaded, installing hook...");
  InstallHook();
}

}// namespace OpenKneeboard
