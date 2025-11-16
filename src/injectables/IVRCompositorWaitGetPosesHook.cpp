// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "IVRCompositorWaitGetPosesHook.hpp"

#include "DllLoadWatcher.hpp"
#include "ScopedRWX.hpp"
#include "detours-ext.hpp"

#include <OpenKneeboard/dprint.hpp>

#include <bit>
#include <mutex>
#include <stdexcept>

namespace OpenKneeboard {

namespace {

const char* MODULE_NAME = "openvr_api.dll";

struct IVRCompositor_VTable {
  void* mSetTrackingSpace;
  void* mGetTrackingSpace;
  void* mWaitGetPoses;
};

decltype(&vr::IVRCompositor::WaitGetPoses) Real_IVRCompositor_WaitGetPoses
  = nullptr;
decltype(&vr::VR_GetGenericInterface) Real_VR_GetGenericInterface = nullptr;

}// namespace

struct IVRCompositorWaitGetPosesHook::Impl {
  Impl(const Callbacks& callbacks)
    : mLibOpenVR(MODULE_NAME), mCallbacks(callbacks) {
  }

  ~Impl() {
    UninstallHook();
  }

  DllLoadWatcher mLibOpenVR {MODULE_NAME};

  IVRCompositor_VTable* mVTable = nullptr;

  void InstallHook();
  void UninstallHook();

  void InstallCompositorHook(vr::IVRCompositor* compositor);
  void InstallVRGetGenericInterfaceHook();

  vr::EVRCompositorError Hooked_IVRCompositor_WaitGetPoses(
    vr::TrackedDevicePose_t* pRenderPoseArray,
    uint32_t unRenderPoseArrayCount,
    vr::TrackedDevicePose_t* pGamePoseArray,
    uint32_t unGamePoseArrayCount);

  static void* VR_CALLTYPE Hooked_VR_GetGenericInterface(
    const char* pchInterfaceVersion,
    vr::EVRInitError* peError);

  Impl() = delete;
  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl& operator=(Impl&&) = delete;

 private:
  static Impl* gInstance;
  Callbacks mCallbacks;
  bool mHookedVRGetGenericInterface = false;
  std::mutex mInstallMutex;
};

IVRCompositorWaitGetPosesHook::Impl*
  IVRCompositorWaitGetPosesHook::Impl::gInstance
  = nullptr;

IVRCompositorWaitGetPosesHook::IVRCompositorWaitGetPosesHook() {
  dprint(__FUNCTION__);
}

void IVRCompositorWaitGetPosesHook::InstallHook(const Callbacks& callbacks) {
  p = std::make_unique<Impl>(callbacks);
  p->InstallHook();
}

void IVRCompositorWaitGetPosesHook::Impl::InstallCompositorHook(
  vr::IVRCompositor* compositor) {
  dprint("Got an OpenVR compositor");
  mVTable = *reinterpret_cast<IVRCompositor_VTable**>(compositor);
  *reinterpret_cast<void**>(&Real_IVRCompositor_WaitGetPoses)
    = mVTable->mWaitGetPoses;

  dprint("Found WaitGetPoses at: {:#018x}", (uint64_t)mVTable->mWaitGetPoses);

  {
    ScopedRWX rwx(mVTable);
    mVTable->mWaitGetPoses
      = std::bit_cast<void*>(&Impl::Hooked_IVRCompositor_WaitGetPoses);
  }

  gInstance = this;
}

IVRCompositorWaitGetPosesHook::~IVRCompositorWaitGetPosesHook() {
  UninstallHook();
}

void IVRCompositorWaitGetPosesHook::UninstallHook() {
  p->UninstallHook();
}

void* VR_CALLTYPE
IVRCompositorWaitGetPosesHook::Impl::Hooked_VR_GetGenericInterface(
  const char* pchInterfaceVersion,
  vr::EVRInitError* peError) {
  auto ret = Real_VR_GetGenericInterface(pchInterfaceVersion, peError);
  if (!gInstance) {
    return ret;
  }

  dprint("Requested OpenVR interface: {}", pchInterfaceVersion);

  if (std::string_view(pchInterfaceVersion).starts_with("IVRCompositor_")) {
    auto p = gInstance;
    p->UninstallHook();
    p->InstallCompositorHook(reinterpret_cast<vr::IVRCompositor*>(ret));
  }

  return ret;
}

void IVRCompositorWaitGetPosesHook::Impl::InstallVRGetGenericInterfaceHook() {
  mHookedVRGetGenericInterface = true;
  DetourSingleAttach(
    &Real_VR_GetGenericInterface, &Hooked_VR_GetGenericInterface);
}

void IVRCompositorWaitGetPosesHook::Impl::UninstallHook() {
  if (gInstance != this) {
    return;
  }

  dprint("Uninstalling OpenVR hooks");

  mLibOpenVR.UninstallHook();

  if (mVTable) {
    ScopedRWX rwx(mVTable);
    mVTable->mWaitGetPoses
      = std::bit_cast<void*>(Real_IVRCompositor_WaitGetPoses);
    mVTable = nullptr;
  }

  if (mHookedVRGetGenericInterface) {
    DetourSingleDetach(
      &Real_VR_GetGenericInterface, &Hooked_VR_GetGenericInterface);
    mHookedVRGetGenericInterface = false;
  }

  gInstance = nullptr;
}

vr::EVRCompositorError
IVRCompositorWaitGetPosesHook::Impl::Hooked_IVRCompositor_WaitGetPoses(
  vr::TrackedDevicePose_t* pRenderPoseArray,
  uint32_t unRenderPoseArrayCount,
  vr::TrackedDevicePose_t* pGamePoseArray,
  uint32_t unGamePoseArrayCount) {
  auto this_ = reinterpret_cast<vr::IVRCompositor*>(this);
  if (!(gInstance && gInstance->mCallbacks.onWaitGetPoses)) [[unlikely]] {
    return std::invoke(
      Real_IVRCompositor_WaitGetPoses,
      this_,
      pRenderPoseArray,
      unRenderPoseArrayCount,
      pGamePoseArray,
      unGamePoseArrayCount);
  }
  return gInstance->mCallbacks.onWaitGetPoses(
    this_,
    pRenderPoseArray,
    unRenderPoseArrayCount,
    pGamePoseArray,
    unGamePoseArrayCount,
    Real_IVRCompositor_WaitGetPoses);
}

void IVRCompositorWaitGetPosesHook::Impl::InstallHook() {
  std::unique_lock lock(mInstallMutex);
  if (gInstance == this) {
    return;
  }

  if (gInstance) {
    throw std::logic_error("Only one IVRCompositorWaitGetPoseHook at a time");
  }

  if (!mLibOpenVR.IsDllLoaded()) {
    return;
  }

  Real_VR_GetGenericInterface
    = reinterpret_cast<decltype(&vr::VR_GetGenericInterface)>(
      DetourFindFunction(MODULE_NAME, "VR_GetGenericInterface"));
  if (Real_VR_GetGenericInterface) {
    dprint("Found OpenVR API");
  } else {
    dprint("Did not find OpenVR API");
    return;
  }

  gInstance = this;

  vr::EVRInitError vrError {};
  auto compositor = reinterpret_cast<vr::IVRCompositor*>(
    Real_VR_GetGenericInterface(vr::IVRCompositor_Version, &vrError));
  if (!compositor) {
    dprint("No OpenVR compositor found: {}", static_cast<int>(vrError));
    dprint("Waiting to see if we get one...");
    this->InstallVRGetGenericInterfaceHook();
    return;
  }

  this->InstallCompositorHook(compositor);
  if (mCallbacks.onHookInstalled) {
    mCallbacks.onHookInstalled();
  }
}

}// namespace OpenKneeboard
