// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/EnumerateProcesses.hpp>
#include <OpenKneeboard/RayIntersectsRect.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>
#include <OpenKneeboard/SteamVRKneeboard.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/task/resume_after.hpp>

#include <shims/winrt/base.h>

#include <d3d11.h>

#include <directxtk/SimpleMath.h>
#include <felly/numeric_cast.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <source_location>
#include <thread>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <openvr.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

SteamVRKneeboard::SteamVRKneeboard()
  : mSHM(SHM::ConsumerKind::OpenVR, mDXR.mD3D11Device.get()) {
  OPENKNEEBOARD_TraceLoggingScope("SteamVRKneeboard::SteamVRKneeboard()");
  auto d3d = mDXR.mD3D11Device.get();
  {
    DXGI_ADAPTER_DESC desc;
    mDXR.mDXGIAdapter->GetDesc(&desc);
    dprint(
      L"SteamVR client running on adapter '{}' (LUID {:#x})",
      desc.Description,
      mDXR.mAdapterLUID);
  }

  winrt::check_hresult(
    d3d->CreateFence(0, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.put())));

  D3D11_TEXTURE2D_DESC desc {
    .Width = MaxViewRenderSize.mWidth,
    .Height = MaxViewRenderSize.mHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
  };
  check_hresult(d3d->CreateTexture2D(&desc, nullptr, mBufferTexture.put()));

  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  for (uint8_t i = 0; i < MaxViewCount; ++i) {
    auto& layer = mLayers.at(i);
    check_hresult(
      d3d->CreateTexture2D(&desc, nullptr, layer.mOpenVRTexture.put()));
    check_hresult(layer.mOpenVRTexture.as<IDXGIResource>()->GetSharedHandle(
      &layer.mSharedHandle));
  }

  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };
  check_hresult(d3d->CreateRenderTargetView(
    mBufferTexture.get(), &rtvd, mRenderTargetView.put()));

  D3D11_TEXTURE2D_DESC beamDesc {
    .Width = 32,
    .Height = 32,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
  };
  check_hresult(d3d->CreateTexture2D(&beamDesc, nullptr, mBeamTexture.put()));
  check_hresult(
    mBeamTexture.as<IDXGIResource>()->GetSharedHandle(&mBeamSharedHandle));

  mSpriteBatch = std::make_unique<D3D11::SpriteBatch>(d3d);
}

SteamVRKneeboard::~SteamVRKneeboard() {
  OPENKNEEBOARD_TraceLoggingScope("SteamVRKneeboard::~SteamVRKneeboard()");
  this->Reset();
}

void SteamVRKneeboard::Reset() {
  OPENKNEEBOARD_TraceLoggingScope("SteamVRKneeboard::Reset()");
  if (!mIVRSystem) {
    return;
  }

  dprint(__FUNCTION__);

  vr::VR_Shutdown();
  mIVRSystem = {};
  mIVROverlay = {};
  mBeamVisible = false;
  mBeamOverlay = {};
  for (auto& layer: mLayers) {
    layer.mVisible = false;
    layer.mOverlay = {};
  }
}

static bool overlay_check(
  vr::EVROverlayError err,
  const char* method,
  const std::source_location& caller = std::source_location::current()) {
  if (err == vr::VROverlayError_None) {
    return true;
  }
  dprint(
    "OpenVR error in IVROverlay::{}: {} @ {}",
    method,
    vr::VROverlay()->GetOverlayErrorNameFromEnum(err),
    caller);
  return false;
}

#define OVERLAY_CHECK(method, ...) \
  if (!overlay_check(mIVROverlay->method(__VA_ARGS__), #method)) { \
    this->Reset(); \
    return; \
  }

bool SteamVRKneeboard::InitializeOpenVR() {
  if (mIVRSystem && mIVROverlay) {
    return true;
  }
  dprint(__FUNCTION__);

  vr::EVRInitError err {};
  if (!mIVRSystem) {
    mIVRSystem = vr::VR_Init(&err, vr::VRApplication_Background);
    if (!mIVRSystem) {
      dprint("Failed to get an OpenVR IVRSystem: {}", static_cast<int>(err));
      return false;
    }

    dprint("Initialized OpenVR, runtime v{}", mIVRSystem->GetRuntimeVersion());

    uint64_t luid {};
    mIVRSystem->GetOutputDevice(&luid, vr::ETextureType::TextureType_DirectX);
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(mDXR.mDXGIFactory->EnumAdapterByLuid(
      std::bit_cast<LUID>(luid), IID_PPV_ARGS(adapter.put())));
    DXGI_ADAPTER_DESC desc;
    winrt::check_hresult(adapter->GetDesc(&desc));

    dprint(
      L"OpenVR requested adapter '{}' (LUID {:#x})", desc.Description, luid);
    if (luid != mDXR.mAdapterLUID) {
      dprint.Warning(
        "SteamVR adapter {:#x} != OKB adapter {:#x}", luid, mDXR.mAdapterLUID);
    }
  }

  if (!mIVROverlay) {
    mIVROverlay = vr::VROverlay();
    if (!mIVROverlay) {
      dprint("Failed to get an OpenVR IVROverlay");
      return false;
    }
    dprint("Initialized OpenVR overlay system");
  }

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
  OPENKNEEBOARD_TraceLoggingScope("SteamVRKneeboard::Tick()");
  vr::VREvent_t event;
  for (const auto& layerState: mLayers) {
    if (!layerState.mOverlay) {
      continue;
    }
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

  const auto frame = mSHM.MaybeGetMapped();
  if (!frame) {
    this->HideAllOverlays();
    return;
  }

  const auto displayTime = this->GetDisplayTime();
  const auto maybeHMDPose = this->GetHMDPose(displayTime);
  if (!maybeHMDPose) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  const float dt = mLastFrameTime
    ? std::chrono::duration<float>(now - *mLastFrameTime).count()
    : 0.016f;
  mLastFrameTime = now;
  const auto hmdPose = *maybeHMDPose;
  const auto effectiveVR = this->GetEffectiveVRSettings(frame->mConfig.mVR);
  const auto effectiveMode = GetEffectiveFollowMode(effectiveVR.mHeadFollow);
  SHM::Config effectiveConfig = frame->mConfig;
  effectiveConfig.mVR = effectiveVR;

  const auto handMountPose = this->GetHandMountPose(effectiveMode, displayTime);
  const uint8_t preferredHand = effectiveVR.mHeadFollow.mPinnedHand == 1 ? 0 : 1;
  const auto controllerInputs =
    this->PollControllerInputs(preferredHand, displayTime);
  this->UpdateControllerManipulation(
    effectiveConfig,
    frame->mLayers,
    hmdPose,
    handMountPose,
    controllerInputs,
    dt);
  effectiveConfig.mVR = this->GetEffectiveVRSettings(frame->mConfig.mVR);
  const auto vrLayers = this->GetLayers(
    effectiveConfig, frame->mLayers, hmdPose, dt, handMountPose);

  auto ctx = mDXR.mD3D11ImmediateContext.get();
  std::vector<size_t> activeLayers;

  const auto baseTint = frame->mConfig.mTint;

  for (size_t layerIndex = 0; layerIndex < vrLayers.size(); ++layerIndex) {
    this->InitializeLayer(layerIndex);
    const auto [layer, renderParams] = vrLayers.at(layerIndex);
    auto& layerState = mLayers.at(layerIndex);

    activeLayers.push_back(layerIndex);

    // Copy the texture as for interoperability with other systems
    // (e.g. DirectX12) we use SHARED_NTHANDLE, but SteamVR doesn't
    // support that - so we need to use a second texture with different
    // sharing parameters.
    //
    // Also lets us apply opacity here, rather than needing another
    // OpenVR call

    // non-atomic paint to buffer...
    ctx->ClearRenderTargetView(
      mRenderTargetView.get(), DirectX::Colors::Transparent);

    const DirectX::XMVECTORF32 layerTint {
      baseTint[0] * renderParams.mKneeboardOpacity,
      baseTint[1] * renderParams.mKneeboardOpacity,
      baseTint[2] * renderParams.mKneeboardOpacity,
      baseTint[3] * renderParams.mKneeboardOpacity,
    };

    const auto& imageSize = layer->mVR.mLocationOnTexture.mSize;
    mSpriteBatch->Begin(mRenderTargetView.get(), MaxViewRenderSize);
    mSpriteBatch->Draw(
      frame->mShaderResourceView,
      layer->mVR.mLocationOnTexture,
      {
        {0, 0},
        layer->mVR.mLocationOnTexture.mSize,
      },
      layerTint);
    const auto& visuals = this->GetManipulationVisuals();
    const bool hasGizmoStrip =
      frame->mConfig.mTextureSize.mWidth >= 128
      && frame->mConfig.mTextureSize.mHeight > 32;
    const bool isTargetLayer =
      layer->mLayerID == visuals.mTargetLayerID
      || (visuals.mTargetLayerID == 0
          && (vrLayers.size() == 1
              || layer->mLayerID == frame->mConfig.mGlobalInputLayerID));
    if (hasGizmoStrip && (visuals.mEditModeActive || visuals.mContentPointerActive) && isTargetLayer) {
      const uint32_t gizmoY = frame->mConfig.mTextureSize.mHeight - 32u;
      const PixelRect gizmoReticleSrc {{32u, gizmoY}, {32u, 32u}};
      const PixelRect gizmoAmberSrc {{64u, gizmoY}, {32u, 32u}};
      const PixelRect gizmoGoldSrc {{96u, gizmoY}, {32u, 32u}};
      const uint32_t minDim = (std::min)(imageSize.mWidth, imageSize.mHeight);
      const uint32_t borderT = std::clamp(minDim / 110u, 3u, 8u);
      const DirectX::XMVECTORF32 gizmoTint {
        baseTint[0], baseTint[1], baseTint[2], baseTint[3]};

      if (visuals.mEditModeActive && !effectiveConfig.mVR.mEditModeEnabled) {
        mSpriteBatch->Draw(
          frame->mShaderResourceView,
          gizmoAmberSrc,
          {{0u, 0u}, {imageSize.mWidth, borderT}},
          gizmoTint);
        mSpriteBatch->Draw(
          frame->mShaderResourceView,
          gizmoAmberSrc,
          {{0u, imageSize.mHeight - borderT}, {imageSize.mWidth, borderT}},
          gizmoTint);
        mSpriteBatch->Draw(
          frame->mShaderResourceView,
          gizmoAmberSrc,
          {{0u, 0u}, {borderT, imageSize.mHeight}},
          gizmoTint);
        mSpriteBatch->Draw(
          frame->mShaderResourceView,
          gizmoAmberSrc,
          {{imageSize.mWidth - borderT, 0u}, {borderT, imageSize.mHeight}},
          gizmoTint);
      }
      if (visuals.mEditModeActive && visuals.mHoveredCorner != EditCorner::None) {
        const uint32_t cSize = std::clamp(minDim / 20u, 18u, 38u);
        const uint32_t cx =
          (visuals.mHoveredCorner == EditCorner::NE
           || visuals.mHoveredCorner == EditCorner::SE)
          ? (imageSize.mWidth > cSize ? imageSize.mWidth - cSize : 0u)
          : 0u;
        const uint32_t cy =
          (visuals.mHoveredCorner == EditCorner::SW
           || visuals.mHoveredCorner == EditCorner::SE)
          ? (imageSize.mHeight > cSize ? imageSize.mHeight - cSize : 0u)
          : 0u;
        mSpriteBatch->Draw(
          frame->mShaderResourceView,
          gizmoGoldSrc,
          {{cx, cy}, {cSize, cSize}},
          gizmoTint);
      }
      if (visuals.mHasHitOnBoard) {
        const uint32_t rad = std::clamp(minDim / 36u, 10u, 22u);
        const uint32_t diam = rad * 2u;
        const uint32_t rawX = static_cast<uint32_t>(
          std::clamp(visuals.mHitLocalU, 0.0f, 1.0f) * imageSize.mWidth);
        const uint32_t rawY = static_cast<uint32_t>(
          std::clamp(visuals.mHitLocalV, 0.0f, 1.0f) * imageSize.mHeight);
        const uint32_t rx = std::clamp(
          rawX > rad ? (rawX - rad) : 0u,
          0u,
          imageSize.mWidth > diam ? (imageSize.mWidth - diam) : 0u);
        const uint32_t ry = std::clamp(
          rawY > rad ? (rawY - rad) : 0u,
          0u,
          imageSize.mHeight > diam ? (imageSize.mHeight - diam) : 0u);
        mSpriteBatch->Draw(
          frame->mShaderResourceView,
          gizmoReticleSrc,
          {{rx, ry}, {diam, diam}},
          gizmoTint);
      }
    }
    mSpriteBatch->End();

    // ... then atomic copy to OpenVR texture
    const D3D11_BOX sourceBox {
      0,
      0,
      0,
      imageSize.mWidth,
      imageSize.mHeight,
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

    layerState.mFenceValue = ++mFenceValue;
    winrt::check_hresult(ctx->Signal(mFence.get(), layerState.mFenceValue));
  }

  for (const auto& layerIndex: activeLayers) {
    const auto& [layer, renderParams] = vrLayers.at(layerIndex);
    auto& layerState = mLayers.at(layerIndex);

    // SteamVR has no synchronization support, so an explicit CPU/GPU sync is
    // needed.
    //
    // If you remove this, test that SteamVR updates when changing tabs/pages
    // when there are not regular page dirty events. For example:
    // - disable/hide the clock/footer
    // - remove any window capture or browser tabs
    winrt::check_hresult(mFence->SetEventOnCompletion(
      layerState.mFenceValue, mGPUFlushEvent.get()));
    {
      OPENKNEEBOARD_TraceLoggingScopedActivity(
        waitActivity, "SteamVRKneeboard::Tick()/WaitForSingleObject");
      const auto result = WaitForSingleObject(mGPUFlushEvent.get(), INFINITE);
      if (result != WAIT_OBJECT_0) {
        const auto error = GetLastError();
        TraceLoggingWriteStop(
          waitActivity,
          "SteamVRKneeboard::Tick()/WaitForSingleObject",
          TraceLoggingValue(result, "Result"),
          TraceLoggingValue(error, "Error"));
        waitActivity.CancelAutoStop();
        OPENKNEEBOARD_BREAK;
      }
    }

    OVERLAY_CHECK(
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

    OVERLAY_CHECK(
      SetOverlayTransformAbsolute,
      layerState.mOverlay,
      vr::TrackingUniverseStanding,
      reinterpret_cast<const vr::HmdMatrix34_t*>(&transform));

    const auto& imageSize = layer->mVR.mLocationOnTexture.mSize;
    vr::VRTextureBounds_t textureBounds {
      0.0f,
      0.0f,
      static_cast<float>(imageSize.mWidth) / MaxViewRenderSize.mWidth,
      static_cast<float>(imageSize.mHeight) / MaxViewRenderSize.mHeight,
    };

    OVERLAY_CHECK(SetOverlayTextureBounds, layerState.mOverlay, &textureBounds);
  }
  for (uint8_t i = 0; i < MaxViewCount; ++i) {
    if (!mLayers.at(i).mOverlay) {
      continue;
    }
    auto& visible = mLayers.at(i).mVisible;
    if (i >= vrLayers.size()) {
      if (visible) {
        OVERLAY_CHECK(HideOverlay, mLayers.at(i).mOverlay);
        visible = false;
      }
      continue;
    }

    const auto enabled = frame->mLayers.at(i).mVREnabled;
    if (visible && !enabled) {
      OVERLAY_CHECK(HideOverlay, mLayers.at(i).mOverlay);
      visible = false;
      continue;
    }

    if (enabled && !visible) {
      OVERLAY_CHECK(ShowOverlay, mLayers.at(i).mOverlay);
      visible = true;
      continue;
    }
  }

  const auto& visuals = this->GetManipulationVisuals();
  const bool hasGizmoStrip =
    frame->mConfig.mTextureSize.mWidth >= 128
    && frame->mConfig.mTextureSize.mHeight > 32;
  const Vector3 beamVec = visuals.mRayEndWorld - visuals.mRayStartWorld;
  const float beamLen = beamVec.Length();
  if (visuals.mHasRay && hasGizmoStrip && beamLen > 0.03f) {
    if (!mBeamOverlay) {
      auto key = std::format("{}.laserbeam", ProjectReverseDomainA);
      OVERLAY_CHECK(
        CreateOverlay, key.c_str(), "OpenKneeboard Laser", &mBeamOverlay);
      vr::Texture_t vrt {
        .handle = mBeamSharedHandle,
        .eType = vr::TextureType_DXGISharedHandle,
        .eColorSpace = vr::ColorSpace_Auto,
      };
      OVERLAY_CHECK(SetOverlayTexture, mBeamOverlay, &vrt);
      OVERLAY_CHECK(
        SetOverlayFlag,
        mBeamOverlay,
        vr::VROverlayFlags_IsPremultiplied,
        SHM::SHARED_TEXTURE_IS_PREMULTIPLIED);
    }
    const uint32_t gizmoY = frame->mConfig.mTextureSize.mHeight - 32u;
    winrt::com_ptr<ID3D11Resource> srcRes;
    frame->mShaderResourceView->GetResource(srcRes.put());
    const D3D11_BOX beamBox {0u, gizmoY, 0u, 32u, gizmoY + 32u, 1u};
    ctx->CopySubresourceRegion(
      mBeamTexture.get(), 0, 0, 0, 0, srcRes.get(), 0, &beamBox);
    const auto beamFence = ++mFenceValue;
    winrt::check_hresult(ctx->Signal(mFence.get(), beamFence));
    winrt::check_hresult(
      mFence->SetEventOnCompletion(beamFence, mGPUFlushEvent.get()));
    WaitForSingleObject(mGPUFlushEvent.get(), INFINITE);

    const Vector3 up = beamVec / beamLen;
    const Vector3 center =
      (visuals.mRayStartWorld + visuals.mRayEndWorld) * 0.5f;
    Vector3 toHmd = hmdPose.mPosition - center;
    if (toHmd.LengthSquared() < 1e-6f) {
      toHmd = Vector3::Backward;
    } else {
      toHmd.Normalize();
    }
    Vector3 right = up.Cross(toHmd);
    if (right.LengthSquared() < 1e-6f) {
      right = Vector3::UnitX;
    } else {
      right.Normalize();
    }
    Vector3 forward = right.Cross(up);
    forward.Normalize();
    Matrix rotMat(
      right.x, right.y, right.z, 0.0f,
      up.x, up.y, up.z, 0.0f,
      forward.x, forward.y, forward.z, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f);
    const Matrix transform =
      (rotMat * Matrix::CreateTranslation(center)).Transpose();
    OVERLAY_CHECK(SetOverlaySortOrder, mBeamOverlay, 1);
    OVERLAY_CHECK(SetOverlayAlpha, mBeamOverlay, 0.72f);
    OVERLAY_CHECK(SetOverlayWidthInMeters, mBeamOverlay, 0.006f);
    OVERLAY_CHECK(SetOverlayTexelAspect, mBeamOverlay, 0.006f / beamLen);
    OVERLAY_CHECK(
      SetOverlayTransformAbsolute,
      mBeamOverlay,
      vr::TrackingUniverseStanding,
      reinterpret_cast<const vr::HmdMatrix34_t*>(&transform));
    if (!mBeamVisible) {
      OVERLAY_CHECK(ShowOverlay, mBeamOverlay);
      mBeamVisible = true;
    }
  } else if (mBeamOverlay && mBeamVisible) {
    OVERLAY_CHECK(HideOverlay, mBeamOverlay);
    mBeamVisible = false;
  }
}

void SteamVRKneeboard::HideAllOverlays() {
  if (mBeamOverlay && mBeamVisible) {
    mBeamVisible = false;
    mIVROverlay->HideOverlay(mBeamOverlay);
  }
  for (auto& layerState: mLayers) {
    if (layerState.mOverlay && layerState.mVisible) {
      layerState.mVisible = false;
      mIVROverlay->HideOverlay(layerState.mOverlay);
    }
  }
}

std::optional<SteamVRKneeboard::Pose> SteamVRKneeboard::GetHandMountPose(
  const VRFollowMode mode,
  const float displayTime) {
  if (
    mode != VRFollowMode::LeftHand && mode != VRFollowMode::RightHand) {
    return std::nullopt;
  }
  if (!mIVRSystem) {
    return std::nullopt;
  }
  const auto role = (mode == VRFollowMode::LeftHand)
    ? vr::TrackedControllerRole_LeftHand
    : vr::TrackedControllerRole_RightHand;
  const auto deviceIndex =
    mIVRSystem->GetTrackedDeviceIndexForControllerRole(role);
  if (
    deviceIndex == vr::k_unTrackedDeviceIndexInvalid
    || deviceIndex >= vr::k_unMaxTrackedDeviceCount) {
    return std::nullopt;
  }
  std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses {};
  mIVRSystem->GetDeviceToAbsoluteTrackingPose(
    vr::TrackingUniverseStanding,
    displayTime,
    poses.data(),
    static_cast<uint32_t>(poses.size()));
  const auto& pose = poses[deviceIndex];
  if (!(pose.bDeviceIsConnected && pose.bPoseIsValid)) {
    return std::nullopt;
  }
  const auto& f = pose.mDeviceToAbsoluteTracking.m;
  // clang-format off
  Matrix m {
    f[0][0], f[1][0], f[2][0], 0,
    f[0][1], f[1][1], f[2][1], 0,
    f[0][2], f[1][2], f[2][2], 0,
    f[0][3], f[1][3], f[2][3], 1
  };
  // clang-format on
  return Pose {
    .mPosition = m.Translation(),
    .mOrientation = Quaternion::CreateFromRotationMatrix(m),
  };
}

std::array<SteamVRKneeboard::ControllerInput, 2>
SteamVRKneeboard::PollControllerInputs(
  const uint8_t preferredHand,
  const float displayTime) {
  std::array<ControllerInput, 2> out {};
  if (!mIVRSystem) {
    return out;
  }

  std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses {};
  mIVRSystem->GetDeviceToAbsoluteTrackingPose(
    vr::TrackingUniverseStanding,
    displayTime,
    poses.data(),
    static_cast<uint32_t>(poses.size()));

  const std::array<uint8_t, 2> order {
    static_cast<uint8_t>(preferredHand % 2),
    static_cast<uint8_t>(1 - (preferredHand % 2)),
  };

  for (size_t slotIdx = 0; slotIdx < 2; ++slotIdx) {
    const uint8_t hand = order[slotIdx];
    const auto role = (hand == 0)
      ? vr::TrackedControllerRole_LeftHand
      : vr::TrackedControllerRole_RightHand;
    const auto deviceIndex =
      mIVRSystem->GetTrackedDeviceIndexForControllerRole(role);
    if (
      deviceIndex == vr::k_unTrackedDeviceIndexInvalid
      || deviceIndex >= vr::k_unMaxTrackedDeviceCount) {
      continue;
    }
    const auto& pose = poses[deviceIndex];
    if (!(pose.bDeviceIsConnected && pose.bPoseIsValid)) {
      continue;
    }
    vr::VRControllerState_t cs {};
    if (!mIVRSystem->GetControllerState(deviceIndex, &cs, sizeof(cs))) {
      continue;
    }
    const auto& f = pose.mDeviceToAbsoluteTracking.m;
    // clang-format off
    Matrix m {
      f[0][0], f[1][0], f[2][0], 0,
      f[0][1], f[1][1], f[2][1], 0,
      f[0][2], f[1][2], f[2][2], 0,
      f[0][3], f[1][3], f[2][3], 1
    };
    // clang-format on
    const bool gripHeld =
      (cs.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip)) != 0
      || cs.rAxis[2].x > 0.55f;
    const bool triggerHeld =
      (cs.ulButtonPressed
       & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
        != 0
      || cs.rAxis[1].x > 0.55f;
    const float stickY = cs.rAxis[0].y;

    out[slotIdx] = {
      .mValid = true,
      .mHandIndex = hand,
      .mAimPose =
        {
          .mPosition = m.Translation(),
          .mOrientation = Quaternion::CreateFromRotationMatrix(m),
        },
      .mGripHeld = gripHeld,
      .mTriggerHeld = triggerHeld,
      .mThumbstickY = stickY,
    };
  }
  return out;
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
  static bool sIsRunning = false;
  OPENKNEEBOARD_TraceLoggingScope("IsSteamVRRunning()");
  // We 'should' just call `vr::VR_Init()` and check the result, but it leaks:
  // https://github.com/ValveSoftware/openvr/issues/310
  //
  // Reproduced with OpenVR v1.16.8 and SteamVR v1.20.4 (latest as of
  // 2022-01-13)
  //
  // Also reproduced with vr::VR_IsHmdPresent()
  const auto processes = EnumerateProcesses();
  if (!processes) [[unlikely]] {
    fatal(
      "EnumerateProceses() failed with {:#x}",
      static_cast<uint32_t>(processes.error()));
  }

  static constexpr std::wstring_view SteamVRExecutable {L"vrmonitor.exe"};

  sIsRunning = false;

  for (auto&& process: processes) {
    if (std::wstring_view(process.pProcessName) == SteamVRExecutable) {
      sIsRunning = true;
      break;
    }
  }

  return sIsRunning;
}

task<void> SteamVRKneeboard::Run(std::stop_token stopToken) {
  if (!vr::VR_IsRuntimeInstalled()) {
    dprint("Stopping OpenVR support, no runtime installed.");
    co_return;
  }

  const auto inactiveSleep = std::chrono::seconds(1);
  const auto frameSleep =
    std::chrono::microseconds((1000 * 1000) / FramesPerSecond);

  dprint("Initializing OpenVR support");

  mGPUFlushEvent.close();
  mGPUFlushEvent = winrt::handle {CreateEventW(nullptr, false, false, nullptr)};

  while (!stopToken.stop_requested()) {
    if (
      IsSteamVRRunning() && vr::VR_IsHmdPresent() && this->InitializeOpenVR()) {
      this->Tick();
      if (this->mIVROverlay) {
        this->mIVROverlay->WaitFrameSync(
          felly::numeric_cast<uint32_t>(frameSleep.count()));
      } else {
        if (!co_await resume_after(frameSleep, stopToken)) {
          co_return;
        }
      }
      continue;
    }

    if (!co_await resume_after(inactiveSleep, stopToken)) {
      co_return;
    }
  }
  dprint("Shutting down OpenVR support - stop requested");

  // Free resources in the same thread we allocated them
  this->Reset();
  dprint("Exiting OpenVR thread");
}

void SteamVRKneeboard::InitializeLayer(size_t layerIndex) {
  if (mLayers.at(layerIndex).mOverlay) {
    return;
  }

  auto& layerState = mLayers.at(layerIndex);
  auto key = std::format("{}.{}", ProjectReverseDomainA, layerIndex);
  auto name = std::format("OpenKneeboard {}", layerIndex + 1);

  OVERLAY_CHECK(CreateOverlay, key.c_str(), name.c_str(), &layerState.mOverlay);

  dprint("Created OpenVR overlay {}", layerIndex);

  vr::Texture_t vrt {
    .handle = layerState.mSharedHandle,
    .eType = vr::TextureType_DXGISharedHandle,
    .eColorSpace = vr::ColorSpace_Auto,
  };

  OVERLAY_CHECK(SetOverlayTexture, layerState.mOverlay, &vrt);
  OVERLAY_CHECK(
    SetOverlayFlag,
    layerState.mOverlay,
    vr::VROverlayFlags_IsPremultiplied,
    SHM::SHARED_TEXTURE_IS_PREMULTIPLIED);
}

}// namespace OpenKneeboard
