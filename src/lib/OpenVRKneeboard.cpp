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
#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/OpenVRKneeboard.h>
#include <OpenKneeboard/RayIntersectsRect.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <TlHelp32.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <openvr.h>
#include <shims/winrt.h>

#include <filesystem>
#include <thread>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OpenVRKneeboard::OpenVRKneeboard() {
  D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  winrt::com_ptr<ID3D11Device> device;
  winrt::check_hresult(D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
#ifdef DEBUG
    D3D11_CREATE_DEVICE_DEBUG,
#else
    0,
#endif
    &level,
    1,
    D3D11_SDK_VERSION,
    device.put(),
    nullptr,
    nullptr));
  mD3D = device.as<ID3D11Device1>();

  D3D11_TEXTURE2D_DESC desc {
    .Width = TextureWidth,
    .Height = TextureHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = {},
    .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
  };

  for (uint8_t i = 0; i < MaxLayers; ++i) {
    winrt::check_hresult(device->CreateTexture2D(
      &desc, nullptr, mLayers.at(i).mOpenVRTexture.put()));
  }

  desc.MiscFlags = {};
  desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
  winrt::check_hresult(
    device->CreateTexture2D(&desc, nullptr, mBufferTexture.put()));

  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };
  winrt::check_hresult(device->CreateRenderTargetView(
    mBufferTexture.get(), &rtvd, mRenderTargetView.put()));
}

OpenVRKneeboard::~OpenVRKneeboard() {
  this->Reset();
}

void OpenVRKneeboard::Reset() {
  if (!mIVRSystem) {
    return;
  }

  vr::VR_Shutdown();
  mIVRSystem = {};
  mIVROverlay = {};
  mLayers = {};
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

bool OpenVRKneeboard::InitializeOpenVR() {
  if (mIVRSystem) {
    return true;
  }

#define CHECK(method, ...) \
  if (!overlay_check(mIVROverlay->method(__VA_ARGS__), #method)) { \
    this->Reset(); \
    return false; \
  }

  vr::EVRInitError err;
  mIVRSystem = vr::VR_Init(&err, vr::VRApplication_Background);
  if (!mIVRSystem) {
    return false;
  }
  dprint("Initialized OpenVR");

  mIVROverlay = vr::VROverlay();
  if (!mIVROverlay) {
    return false;
  }
  dprint("Initialized OpenVR overlay system");

  for (uint8_t layerIndex = 0; layerIndex < MaxLayers; ++layerIndex) {
    auto& layerData = mLayers.at(layerIndex);
    auto key = std::format("{}.{}", ProjectNameA, layerIndex);
    auto name = std::format("OpenKneeboard {}", layerIndex + 1);

    CHECK(CreateOverlay, key.c_str(), name.c_str(), &layerData.mOverlay);

    dprintf("Created OpenVR overlay {}", layerIndex);

    HANDLE handle = INVALID_HANDLE_VALUE;
    winrt::check_hresult(
      layerData.mOpenVRTexture.as<IDXGIResource>()->GetSharedHandle(&handle));
    vr::Texture_t vrt {
      .handle = handle,
      .eType = vr::TextureType_DXGISharedHandle,
      .eColorSpace = vr::ColorSpace_Auto,
    };

    CHECK(SetOverlayTexture, layerData.mOverlay, &vrt);
    CHECK(
      SetOverlayFlag,
      layerData.mOverlay,
      vr::VROverlayFlags_IsPremultiplied,
      SHM::SHARED_TEXTURE_IS_PREMULTIPLIED);
  }
#undef CHECK
  return true;
}

float OpenVRKneeboard::GetDisplayTime() {
  return 0.0f;// FIXME
}

void OpenVRKneeboard::Tick() {
#define CHECK(method, ...) \
  if (!overlay_check(mIVROverlay->method(__VA_ARGS__), #method)) { \
    this->Reset(); \
    return; \
  }

  vr::VREvent_t event;
  for (const auto& layer: mLayers) {
    while (mIVROverlay->PollNextOverlayEvent(
      layer.mOverlay, &event, sizeof(event))) {
      if (event.eventType == vr::VREvent_Quit) {
        dprint("OpenVR shutting down, detaching");
        this->Reset();
        return;
      }
    }
  }

  mFrameCounter++;

  auto snapshot = mSHM.MaybeGet();
  if (!snapshot.IsValid()) {
    return;
  }

  const auto displayTime = this->GetDisplayTime();
  const auto hmdPose = this->GetHMDPose(displayTime);

  const auto config = snapshot.GetConfig();

  for (uint8_t layerIndex = 0; layerIndex < snapshot.GetLayerCount();
       ++layerIndex) {
    const auto& layer = *snapshot.GetLayerConfig(layerIndex);
    auto& layerData = mLayers.at(layerIndex);
    if (!layer.IsValid()) {
      if (layerData.mVisible) {
        CHECK(HideOverlay, layerData.mOverlay);
        layerData.mVisible = false;
      }
      continue;
    }

    const auto renderParams
      = this->GetRenderParameters(snapshot, layer, hmdPose);

    if (renderParams.mCacheKey == layerData.mCacheKey) {
      continue;
    }
    CHECK(
      SetOverlayWidthInMeters,
      layerData.mOverlay,
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
      layerData.mOverlay,
      vr::TrackingUniverseStanding,
      reinterpret_cast<const vr::HmdMatrix34_t*>(&transform));

    // Copy the texture as for interoperability with other systems
    // (e.g. DirectX12) we use SHARED_NTHANDLE, but SteamVR doesn't
    // support that - so we need to use a second texture with different
    // sharing parameters.
    //
    // Also lets us apply opacity here, rather than needing another
    // OpenVR call

    {
      auto openKneeboardTexture
        = snapshot.GetLayerTexture(mD3D.get(), layerIndex);

      // non-atomic paint to buffer...
      D3D11::CopyTextureWithOpacity(
        mD3D.get(),
        openKneeboardTexture.GetShaderResourceView(),
        mRenderTargetView.get(),
        renderParams.mKneeboardOpacity);

      // ... then atomic copy to OpenVR texture
      winrt::com_ptr<ID3D11DeviceContext> ctx;
      mD3D->GetImmediateContext(ctx.put());
      ctx->CopyResource(layerData.mOpenVRTexture.get(), mBufferTexture.get());
    }

    vr::VRTextureBounds_t textureBounds {
      0.0f,
      0.0f,
      static_cast<float>(layer.mImageWidth) / TextureWidth,
      static_cast<float>(layer.mImageHeight) / TextureHeight,
    };

    CHECK(SetOverlayTextureBounds, layerData.mOverlay, &textureBounds);

    if (!layerData.mVisible) {
      CHECK(ShowOverlay, layerData.mOverlay);
      layerData.mVisible = true;
    }

    layerData.mCacheKey = renderParams.mCacheKey;
  }
  for (uint8_t i = snapshot.GetLayerCount(); i < MaxLayers; ++i) {
    if (mLayers.at(i).mVisible) {
      CHECK(HideOverlay, mLayers.at(i).mOverlay);
      mLayers.at(i).mVisible = false;
    }
  }

#undef CHECK
}

OpenVRKneeboard::YOrigin OpenVRKneeboard::GetYOrigin() {
  // Always use floor level due to
  // https://github.com/ValveSoftware/openvr/issues/830
  return YOrigin::FLOOR_LEVEL;
}

OpenVRKneeboard::Pose OpenVRKneeboard::GetHMDPose(float displayTime) {
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

bool OpenVRKneeboard::Run(std::stop_token stopToken) {
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
    if (!IsSteamVRRunning()) {
      std::this_thread::sleep_for(inactiveSleep);
      continue;
    }

    if (this->InitializeOpenVR()) {
      this->Tick();
    }
    std::this_thread::sleep_for(mIVRSystem ? frameSleep : inactiveSleep);
  }
  dprint("Shutting down OpenVR support - stop requested");

  // Free resources in the same thread we allocated them
  this->Reset();

  return true;
}

}// namespace OpenKneeboard
