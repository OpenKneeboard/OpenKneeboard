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
#include <OpenKneeboard/config.h>
#include <d3d11_1.h>
#include <openvr.h>
#include <shims/winrt/base.h>

#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace OpenKneeboard {

class OpenVRKneeboard final : private VRKneeboard {
 public:
  OpenVRKneeboard();
  ~OpenVRKneeboard();

  bool Run(std::stop_token);

 protected:
  std::optional<Pose> GetHMDPose(float);

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
  SHM::Reader mSHM;

  // Paint to buffer texture with variable opacity,
  // then atomically copy to OpenVR texture
  winrt::com_ptr<ID3D11Texture2D> mBufferTexture;
  winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;

  struct LayerState {
    bool mVisible = false;
    winrt::com_ptr<ID3D11Texture2D> mOpenVRTexture;
    vr::VROverlayHandle_t mOverlay {};
    uint64_t mCacheKey = ~(0ui64);
    // *NOT* an NT handle. Do not use CloseHandle() or winrt::Handle
    HANDLE mSharedHandle {};
  };
  std::array<LayerState, MaxLayers> mLayers;
};

}// namespace OpenKneeboard
