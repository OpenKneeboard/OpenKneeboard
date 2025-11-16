// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include "OculusEndFrameHook.hpp"

#include "DllLoadWatcher.hpp"
#include "OVRRuntimeDLLNames.hpp"
#include "detours-ext.hpp"

#include <OpenKneeboard/dprint.hpp>

#include <array>
#include <mutex>
#include <stdexcept>

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
#define IT(x) \
  static_assert(std::same_as<decltype(x), decltype(ovr_EndFrame)>); \
  HOOKED_ENDFRAME_FUNCS
#undef IT

namespace OpenKneeboard {

struct OculusEndFrameHook::Impl {
  Impl(const Callbacks&);
  ~Impl();

  void InstallHook(std::shared_ptr<DllLoadWatcher> runtime);
  void UninstallHook();

  Impl() = delete;
  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl& operator=(Impl&&) = delete;

 private:
  static Impl* gInstance;

  std::vector<std::shared_ptr<DllLoadWatcher>> mRuntimes;
  std::weak_ptr<DllLoadWatcher> mActiveRuntime;

  Callbacks mCallbacks;
  std::mutex mInstallMutex;

#define IT(x) \
  static decltype(&x) next_##x; \
  static ovrResult __cdecl x##_hook( \
    ovrSession session, \
    long long frameIndex, \
    const ovrViewScaleDesc* viewScaleDesc, \
    ovrLayerHeader const* const* layerPtrList, \
    unsigned int layerCount); \
  static_assert(std::same_as<decltype(&x), decltype(&x##_hook)>);
  HOOKED_ENDFRAME_FUNCS
#undef IT
};

OculusEndFrameHook::Impl* OculusEndFrameHook::Impl::gInstance = nullptr;

OculusEndFrameHook::OculusEndFrameHook() {
  dprint(__FUNCTION__);
}

void OculusEndFrameHook::InstallHook(const Callbacks& cb) {
  p = std::make_unique<Impl>(cb);
}

OculusEndFrameHook::~OculusEndFrameHook() {
  // Must be called before any other stuff gets deallocated
  this->UninstallHook();
}

void OculusEndFrameHook::UninstallHook() {
  if (p) {
    p->UninstallHook();
  }
}

OculusEndFrameHook::Impl::Impl(const Callbacks& cb) : mCallbacks(cb) {
  for (const auto runtime: OVRRuntimeDLLNames) {
    auto watcher = std::make_shared<DllLoadWatcher>(runtime);
    watcher->InstallHook({
      .onDllLoaded =
        [weak = std::weak_ptr {watcher}, this]() {
          if (auto watcher = weak.lock()) {
            this->InstallHook(watcher);
          }
        },
    });
    this->InstallHook(watcher);
    mRuntimes.push_back(std::move(watcher));
  }
}

void OculusEndFrameHook::Impl::InstallHook(
  std::shared_ptr<DllLoadWatcher> runtime) {
  std::unique_lock lock(mInstallMutex);
  if (!runtime->IsDllLoaded()) {
    return;
  }

  if (gInstance == this) {
    return;
  }

  if (gInstance) {
    throw std::logic_error("Can only have one OculusEndFrameHook at a time");
  }
  gInstance = this;

  // Find outside of the transaction as DetourFindFunction calls LoadLibrary
#define IT(x) \
  next_##x = reinterpret_cast<decltype(&x)>( \
    DetourFindFunction(runtime->GetDLLName(), #x));
  HOOKED_ENDFRAME_FUNCS
#undef IT

  {
    DetourTransaction dt;
#define IT(x) DetourAttach(&next_##x, x##_hook);
    HOOKED_ENDFRAME_FUNCS
#undef IT
  }

  mActiveRuntime = runtime;

  {
    const auto handle = GetModuleHandleA(runtime->GetDLLName());
    if (handle) {
      wchar_t path[1024];
      const auto pathLength = GetModuleFileNameW(handle, path, std::size(path));
      dprint(L"LibOVR runtime path: {}", std::wstring_view {path, pathLength});
    } else {
      dprint("Have LibOVR runtime, but couldn't determine path");
    }
  }

  dprint("Attached OculusEndFrameHook");

  if (mCallbacks.onHookInstalled) {
    mCallbacks.onHookInstalled();
  }
}

OculusEndFrameHook::Impl::~Impl() {
  this->UninstallHook();
}

void OculusEndFrameHook::Impl::UninstallHook() {
  if (gInstance != this) {
    return;
  }
  auto runtime = mActiveRuntime.lock();
  if (!runtime) {
    return;
  }

  runtime->UninstallHook();

  {
    DetourTransaction dt;
#define IT(x) \
  DetourDetach( \
    reinterpret_cast<void**>(&next_##x), reinterpret_cast<void*>(x##_hook));
    HOOKED_ENDFRAME_FUNCS
#undef IT
  }
  gInstance = nullptr;
  dprint("Detached OculusEndFrameHook");
}

#define IT(x) \
  decltype(&x) OculusEndFrameHook::Impl::next_##x = nullptr; \
  ovrResult OculusEndFrameHook::Impl::x##_hook( \
    ovrSession session, \
    long long frameIndex, \
    const ovrViewScaleDesc* viewScaleDesc, \
    ovrLayerHeader const* const* layerPtrList, \
    unsigned int layerCount) { \
    if (gInstance && gInstance->mCallbacks.onEndFrame) [[likely]] { \
      return gInstance->mCallbacks.onEndFrame( \
        session, \
        frameIndex, \
        viewScaleDesc, \
        layerPtrList, \
        layerCount, \
        next_##x); \
    } \
    return next_##x( \
      session, frameIndex, viewScaleDesc, layerPtrList, layerCount); \
  }
HOOKED_ENDFRAME_FUNCS
#undef IT

}// namespace OpenKneeboard
