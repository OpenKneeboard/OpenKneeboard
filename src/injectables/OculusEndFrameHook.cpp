/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include "OculusEndFrameHook.h"

#include "DllLoadWatcher.h"
#include "detours-ext.h"

#include <OpenKneeboard/dprint.h>

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

static const char* MODULE_NAME = "LibOVRRT64_1.dll";

struct OculusEndFrameHook::Impl {
  Impl(const Callbacks&);
  ~Impl();

  void InstallHook();
  void UninstallHook();

 private:
  static Impl* gInstance;
  DllLoadWatcher mLibOVR {MODULE_NAME};
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

OculusEndFrameHook::Impl::Impl(const Callbacks& cb)
  : mLibOVR(MODULE_NAME), mCallbacks(cb) {
  mLibOVR.InstallHook({
    .onDllLoaded = std::bind_front(&Impl::InstallHook, this),
  });
  this->InstallHook();
}

void OculusEndFrameHook::Impl::InstallHook() {
  std::unique_lock lock(mInstallMutex);
  if (!mLibOVR.IsDllLoaded()) {
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
  next_##x \
    = reinterpret_cast<decltype(&x)>(DetourFindFunction(MODULE_NAME, #x));
  HOOKED_ENDFRAME_FUNCS
#undef IT

  {
    DetourTransaction dt;
#define IT(x) DetourAttach(&next_##x, x##_hook);
    HOOKED_ENDFRAME_FUNCS
#undef IT
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
  mLibOVR.UninstallHook();

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
