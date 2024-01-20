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
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/RayIntersectsRect.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SHM/ActiveConsumers.h>
#include <OpenKneeboard/SteamVRKneeboard.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <shims/filesystem>
#include <shims/winrt/base.h>

#include <DirectXTK/SimpleMath.h>

#include <thread>

#include <TlHelp32.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <openvr.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

SteamVRKneeboard::SteamVRKneeboard() {
  {
    // Use DXResources to share the GPU selection logic
    auto dxr = DXResources::Create();
    mD3D = dxr.mD3DDevice;
    mSHM.InitializeCache(mD3D.get(), /* swapchainLength = */ 2);
    winrt::com_ptr<IDXGIAdapter> adapter;
    dxr.mDXGIDevice->GetAdapter(adapter.put());
    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);
    dprintf(
      L"SteamVR running on adapter '{}' (LUID {:#x})",
      desc.Description,
      std::bit_cast<uint64_t>(desc.AdapterLuid));
  }

  D3D11_TEXTURE2D_DESC desc {
    .Width = TextureWidth,
    .Height = TextureHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
  };
  winrt::check_hresult(
    mD3D->CreateTexture2D(&desc, nullptr, mBufferTexture.put()));

  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  for (uint8_t i = 0; i < MaxLayers; ++i) {
    auto& layer = mLayers.at(i);
    winrt::check_hresult(
      mD3D->CreateTexture2D(&desc, nullptr, layer.mOpenVRTexture.put()));
    winrt::check_hresult(
      layer.mOpenVRTexture.as<IDXGIResource>()->GetSharedHandle(
        &layer.mSharedHandle));
  }

  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };
  winrt::check_hresult(mD3D->CreateRenderTargetView(
    mBufferTexture.get(), &rtvd, mRenderTargetView.put()));
}

SteamVRKneeboard::~SteamVRKneeboard() {
  this->Reset();
}

void SteamVRKneeboard::Reset() {
  if (!mIVRSystem) {
    return;
  }

  dprint(__FUNCTION__);

  vr::VR_Shutdown();
  mIVRSystem = {};
  mIVROverlay = {};
  for (auto& layer: mLayers) {
    layer.mVisible = false;
    layer.mOverlay = {};
  }
}

static bool overlay_check(vr::EVROverlayError err, const char* method) {
  if (err == vr::VROverlayError_None) {
    return true;
  }
  dprintf(
    "OpenVR error in IVROverlay::{}: {}",
    method,
    vr::VROverlay()->GetOverlayErrorNameFromEnum(err));
  return false;
}

bool SteamVRKneeboard::InitializeOpenVR() {
  if (mIVRSystem && mIVROverlay) {
    return true;
  }
  dprint(__FUNCTION__);

#define CHECK(method, ...) \
  if (!overlay_check(mIVROverlay->method(__VA_ARGS__), #method)) { \
    this->Reset(); \
    return false; \
  }

  vr::EVRInitError err;
  if (!mIVRSystem) {
    mIVRSystem = vr::VR_Init(&err, vr::VRApplication_Background);
    if (!mIVRSystem) {
      dprintf("Failed to get an OpenVR IVRSystem: {}", static_cast<int>(err));
      return false;
    }
    dprint("Initialized OpenVR");
  }

  if (!mIVROverlay) {
    mIVROverlay = vr::VROverlay();
    if (!mIVROverlay) {
      dprint("Failed to get an OpenVR IVROverlay");
      return false;
    }
    dprint("Initialized OpenVR overlay system");
  }

  for (uint8_t layerIndex = 0; layerIndex < MaxLayers; ++layerIndex) {
    auto& layerState = mLayers.at(layerIndex);
    auto key = std::format("{}.{}", ProjectNameA, layerIndex);
    auto name = std::format("OpenKneeboard {}", layerIndex + 1);

    CHECK(CreateOverlay, key.c_str(), name.c_str(), &layerState.mOverlay);

    dprintf("Created OpenVR overlay {}", layerIndex);

    vr::Texture_t vrt {
      .handle = layerState.mSharedHandle,
      .eType = vr::TextureType_DXGISharedHandle,
      .eColorSpace = vr::ColorSpace_Auto,
    };

    CHECK(SetOverlayTexture, layerState.mOverlay, &vrt);
    CHECK(
      SetOverlayFlag,
      layerState.mOverlay,
      vr::VROverlayFlags_IsPremultiplied,
      SHM::SHARED_TEXTURE_IS_PREMULTIPLIED);
  }
#undef CHECK
  return true;
}

float SteamVRKneeboard::GetDisplayTime() {
  return 0.0f;// FIXME
}

static bool IsOtherVRActive() {
  const auto now = SHM::ActiveConsumers::Clock::now();
  const auto interval = std::chrono::milliseconds(500);
  const auto consumers = SHM::ActiveConsumers::Get();
  return (now - consumers.VRExceptSteam()) <= interval;
}

void SteamVRKneeboard::Tick() {
#define CHECK(method, ...) \
  if (!overlay_check(mIVROverlay->method(__VA_ARGS__), #method)) { \
    this->Reset(); \
    return; \
  }

  vr::VREvent_t event;
  for (const auto& layerState: mLayers) {
    while (mIVROverlay->PollNextOverlayEvent(
      layerState.mOverlay, &event, sizeof(event))) {
      if (event.eventType == vr::VREvent_Quit) {
        dprint("OpenVR shutting down, detaching");
        this->Reset();
        return;
      }
    }
  }

  mFrameCounter++;
  if (!mSHM) {
    this->HideAllOverlays();
    return;
  }

  if (IsOtherVRActive()) {
    this->HideAllOverlays();
    return;
  }

  const auto snapshot = mSHM.MaybeGet();
  if (!snapshot.IsValid()) {
    this->HideAllOverlays();
    return;
  }

  const auto displayTime = this->GetDisplayTime();
  const auto maybeHMDPose = this->GetHMDPose(displayTime);
  if (!maybeHMDPose) {
    return;
  }
  const auto hmdPose = *maybeHMDPose;

  const auto config = snapshot.GetConfig();

  auto srv
    = snapshot.GetTexture<SHM::D3D11::Texture>()->GetD3D11ShaderResourceView();
  if (!srv) {
    dprint("Failed to get shared texture");
    return;
  }

  for (uint8_t layerIndex = 0; layerIndex < snapshot.GetLayerCount();
       ++layerIndex) {
    const auto& layer = *snapshot.GetLayerConfig(layerIndex);
    auto& layerState = mLayers.at(layerIndex);

    const auto renderParams
      = this->GetRenderParameters(snapshot, layer, hmdPose);

    if (renderParams.mCacheKey == layerState.mCacheKey) {
      continue;
    }
    CHECK(
      SetOverlayWidthInMeters,
      layerState.mOverlay,
      renderParams.mKneeboardSize.x);

    // Transpose to fit OpenVR's in-memory layout
    // clang-format off
    const auto transform = (
      Matrix::CreateFromQuaternion(renderParams.mKneeboardPose.mOrientation)
      * Matrix::CreateTranslation(renderParams.mKneeboardPose.mPosition)
    ).Transpose();
    // clang-format on

    CHECK(
      SetOverlayTransformAbsolute,
      layerState.mOverlay,
      vr::TrackingUniverseStanding,
      reinterpret_cast<const vr::HmdMatrix34_t*>(&transform));

    // Copy the texture as for interoperability with other systems
    // (e.g. DirectX12) we use SHARED_NTHANDLE, but SteamVR doesn't
    // support that - so we need to use a second texture with different
    // sharing parameters.
    //
    // Also lets us apply opacity here, rather than needing another
    // OpenVR call

    // non-atomic paint to buffer...
    D3D11::CopyTextureWithOpacity(
      mD3D.get(), srv, mRenderTargetView.get(), renderParams.mKneeboardOpacity);

    // ... then atomic copy to OpenVR texture
    winrt::com_ptr<ID3D11DeviceContext> ctx;
    mD3D->GetImmediateContext(ctx.put());
    const D2D_RECT_U sourceRect = layer.mLocationOnTexture;
    const D3D11_BOX sourceBox {
      sourceRect.left,
      sourceRect.top,
      0,
      sourceRect.right,
      sourceRect.bottom,
      1,
    };
    ctx->CopySubresourceRegion(
      layerState.mOpenVRTexture.get(),
      0,
      0,
      0,
      0,
      mBufferTexture.get(),
      0,
      &sourceBox);
    layerState.mTextureCacheKey = snapshot.GetRenderCacheKey();

    const auto& imageSize = layer.mLocationOnTexture.mSize;
    vr::VRTextureBounds_t textureBounds {
      0.0f,
      0.0f,
      static_cast<float>(imageSize.mWidth) / TextureWidth,
      static_cast<float>(imageSize.mHeight) / TextureHeight,
    };

    CHECK(SetOverlayTextureBounds, layerState.mOverlay, &textureBounds);

    if (!layerState.mVisible) {
      CHECK(ShowOverlay, layerState.mOverlay);
      layerState.mVisible = true;
    }

    layerState.mCacheKey = renderParams.mCacheKey;
  }
  for (uint8_t i = snapshot.GetLayerCount(); i < MaxLayers; ++i) {
    if (mLayers.at(i).mVisible) {
      CHECK(HideOverlay, mLayers.at(i).mOverlay);
      mLayers.at(i).mVisible = false;
    }
  }

#undef CHECK
}

void SteamVRKneeboard::HideAllOverlays() {
  for (auto& layerState: mLayers) {
    if (layerState.mVisible) {
      layerState.mVisible = false;
      mIVROverlay->HideOverlay(layerState.mOverlay);
    }
  }
}

std::optional<SteamVRKneeboard::Pose> SteamVRKneeboard::GetHMDPose(
  float displayTime) {
  static uint64_t sCacheKey = ~(0ui64);
  static Pose sCache {};

  if (this->mFrameCounter == sCacheKey) {
    return sCache;
  }

  vr::TrackedDevicePose_t hmdPose {
    .bPoseIsValid = false,
    .bDeviceIsConnected = false,
  };
  mIVRSystem->GetDeviceToAbsoluteTrackingPose(
    vr::TrackingUniverseStanding, displayTime, &hmdPose, 1);
  if (!(hmdPose.bDeviceIsConnected && hmdPose.bPoseIsValid)) {
    return {};
  }

  const auto& f = hmdPose.mDeviceToAbsoluteTracking.m;
  // clang-format off
  Matrix m {
    f[0][0], f[1][0], f[2][0], 0,
    f[0][1], f[1][1], f[2][1], 0,
    f[0][2], f[1][2], f[2][2], 0,
    f[0][3], f[1][3], f[2][3], 1
  };
  // clang-format on
  sCache = {
    .mPosition = m.Translation(),
    .mOrientation = Quaternion::CreateFromRotationMatrix(m),
  };
  sCacheKey = mFrameCounter;
  return sCache;
}// namespace OpenKneeboard

static bool IsSteamVRRunning() {
  // We 'should' just call `vr::VR_Init()` and check the result, but it leaks:
  // https://github.com/ValveSoftware/openvr/issues/310
  //
  // Reproduced with OpenVR v1.16.8 and SteamVR v1.20.4 (latest as of
  // 2022-01-13)
  //
  // Also reproduced with vr::VR_IsHmdPresent()
  winrt::handle snapshot {CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
  if (!snapshot) {
    dprint("Failed to get a snapshot");
    return false;
  }
  PROCESSENTRY32 process;
  process.dwSize = sizeof(process);
  if (!Process32First(snapshot.get(), &process)) {
    return false;
  }
  do {
    if (std::wstring_view(process.szExeFile) == L"vrmonitor.exe") {
      return true;
    }
  } while (Process32Next(snapshot.get(), &process));
  return false;
}

bool SteamVRKneeboard::Run(std::stop_token stopToken) {
  if (!vr::VR_IsRuntimeInstalled()) {
    dprint("Stopping OpenVR support, no runtime installed.");
    return true;
  }

  if (!mD3D) {
    dprint("Stopping OpenVR support, failed to get D3D11 device");
    return false;
  }

  const auto inactiveSleep = std::chrono::seconds(1);
  const auto frameSleep = std::chrono::milliseconds(1000 / 90);

  dprint("Initializing OpenVR support");

  while (!stopToken.stop_requested()) {
    if (
      IsSteamVRRunning() && vr::VR_IsHmdPresent() && this->InitializeOpenVR()) {
      this->Tick();
      if (this->mIVROverlay) {
        this->mIVROverlay->WaitFrameSync(frameSleep.count());
      } else {
        std::this_thread::sleep_for(frameSleep);
      }
      continue;
    }

    std::this_thread::sleep_for(inactiveSleep);
  }
  dprint("Shutting down OpenVR support - stop requested");

  // Free resources in the same thread we allocated them
  this->Reset();

  return true;
}

}// namespace OpenKneeboard
