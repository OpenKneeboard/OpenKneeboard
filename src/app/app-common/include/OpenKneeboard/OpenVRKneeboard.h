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

#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/VRKneeboard.h>
#include <d3d11_1.h>
#include <openvr.h>
#include <shims/winrt.h>

#include <memory>
#include <stop_token>
#include <vector>

namespace OpenKneeboard {

class OpenVRKneeboard final : private VRKneeboard {
 public:
  OpenVRKneeboard();
  ~OpenVRKneeboard();

  bool Run(std::stop_token);

 protected:
  Pose GetHMDPose(float);
  YOrigin GetYOrigin() override;

 private:
  using Matrix = DirectX::SimpleMath::Matrix;
  float GetDisplayTime();
  bool InitializeOpenVR();
  void Tick();
  void Reset();

  winrt::com_ptr<ID3D11Device1> mD3D;
  uint64_t mFrameCounter = 0;
  vr::IVRSystem* mIVRSystem = nullptr;
  vr::IVROverlay* mIVROverlay = nullptr;
  vr::VROverlayHandle_t mOverlay {};
  bool mVisible = true;
  SHM::Reader mSHM;

  winrt::com_ptr<ID3D11Texture2D> mOpenVRTexture;
  uint64_t mSequenceNumber = ~(0ui64);
};

}// namespace OpenKneeboard
