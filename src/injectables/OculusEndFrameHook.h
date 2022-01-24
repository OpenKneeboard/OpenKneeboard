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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#pragma once

#include <OVR_CAPI.H>

#include <functional>
#include <memory>

namespace OpenKneeboard {

/** Hook for `ovrEndFrame`/`ovrSubmitFrame`/`ovrSubmitFrame2`.
 *
 * These all have the same signature and serve a similar purpose. Each app
 * should only use one of these - which one depends on which version of the
 * SDK they're using, and if they're using the current best practices.
 *
 * The end frame callback will be invoked for all of them, though the `next`
 * parameter might be pointing at `ovrSubmitFrame` or `ovrSubmitFrame2` instead
 * of `ovrEndFrame`.
 */
class OculusEndFrameHook final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  OculusEndFrameHook();
  ~OculusEndFrameHook();

  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<ovrResult(
      ovrSession session,
      long long frameIndex,
      const ovrViewScaleDesc* viewScaleDesc,
      ovrLayerHeader const* const* layerPtrList,
      unsigned int layerCount,
      const decltype(&ovr_EndFrame)& next)>
      onEndFrame;
  };

  void InstallHook(const Callbacks&);
  void UninstallHook();
};

}// namespace OpenKneeboard
