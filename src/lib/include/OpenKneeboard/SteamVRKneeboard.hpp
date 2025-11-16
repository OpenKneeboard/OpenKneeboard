// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/SHM/D3D11.hpp>
#include <OpenKneeboard/VRKneeboard.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <directxtk/SimpleMath.h>

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

  task<void> Run(std::stop_token);

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
  SHM::D3D11::CachedReader mSHM {SHM::ConsumerKind::OpenVR};

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
