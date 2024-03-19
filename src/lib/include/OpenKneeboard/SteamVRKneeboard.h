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

#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SHM/D3D11.h>
#include <OpenKneeboard/VRKneeboard.h>

#include <OpenKneeboard/config.h>

#include <shims/winrt/base.h>

#include <DirectXTK/SimpleMath.h>

#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

#include <d3d11_1.h>
#include <openvr.h>

namespace OpenKneeboard {

class SteamVRKneeboard final : private VRKneeboard {
 public:
  SteamVRKneeboard();
  ~SteamVRKneeboard();

  winrt::Windows::Foundation::IAsyncAction Run(std::stop_token);

 protected:
  std::optional<Pose> GetHMDPose(float);

 private:
  using Matrix = DirectX::SimpleMath::Matrix;
  float GetDisplayTime();
  bool InitializeOpenVR();
  void Tick();
  void Reset();
  void HideAllOverlays();

  // Superclass of DXResources, but naming it like this for
  // consistency/familiarity in the code
  D3D11Resources mDXR;
  uint64_t mFrameCounter = 0;
  vr::IVRSystem* mIVRSystem = nullptr;
  vr::IVROverlay* mIVROverlay = nullptr;
  SHM::D3D11::CachedReader mSHM {SHM::ConsumerKind::SteamVR};

  std::unique_ptr<D3D11::SpriteBatch> mSpriteBatch;

  // Paint to buffer texture with variable opacity,
  // then atomically copy to OpenVR texture
  winrt::com_ptr<ID3D11Texture2D> mBufferTexture;
  winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
  winrt::com_ptr<ID3D11Fence> mFence;
  uint64_t mFenceValue {};
  winrt::handle mGPUFlushEvent;

  struct LayerState {
    bool mVisible = false;
    winrt::com_ptr<ID3D11Texture2D> mOpenVRTexture;
    vr::VROverlayHandle_t mOverlay {};
    uint64_t mCacheKey = ~(0ui64);
    uint64_t mFenceValue {};
    // *NOT* an NT handle. Do not use CloseHandle() or winrt::Handle
    HANDLE mSharedHandle {};
  };
  std::array<LayerState, MaxViewCount> mLayers;

  void InitializeLayer(size_t index);
};

}// namespace OpenKneeboard
