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