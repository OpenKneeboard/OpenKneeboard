// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/SHM.hpp>

#include <chrono>

namespace OpenKneeboard::SHM {
struct ActiveConsumers final {
  using Clock = std::chrono::steady_clock;
  using T = Clock::time_point;

  // These should be kept in sync with `SHM::ConsumerKind`.
  T mOpenVR {};
  T mOpenXR {};
  T mOculusD3D11 {};
  T mNonVRD3D11 {};
  T mViewer {};

  T AnyVR() const;
  T VRExceptSteam() const;
  T NotVR() const;
  T NotVROrViewer() const;
  T Any() const;

  DWORD mElevatedConsumerProcessID {};

  PixelSize mNonVRPixelSize {};
  uint64_t mActiveInGameViewID {};

  static void Clear();
  static ActiveConsumers Get();
  static void Set(ConsumerKind);
  static void SetNonVRPixelSize(PixelSize px);
  static void SetActiveInGameViewID(uint64_t);

 private:
  class Impl;
};
static_assert(std::is_standard_layout_v<ActiveConsumers>);

}// namespace OpenKneeboard::SHM