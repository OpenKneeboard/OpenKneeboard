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

#include <filesystem>
#include <source_location>
#include <thread>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <openvr.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

SteamVRKneeboard::SteamVRKneeboard() {
  OPENKNEEBOARD_TraceLoggingScope("SteamVRKneeboard::SteamVRKneeboard()");
  auto d3d = mDXR.mD3D11Device.get();
  {
    mSHM.InitializeCache(d3d, /* swapchainLength = */ 2);
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

  const auto snapshot = mSHM.MaybeGet();
  if (!snapshot.HasTexture()) {
    this->HideAllOverlays();
    return;
  }

  const auto displayTime = this->GetDisplayTime();
  const auto maybeHMDPose = this->GetHMDPose(displayTime);
  if (!maybeHMDPose) {
    return;
  }
  const auto hmdPose = *maybeHMDPose;
  const auto vrLayers = this->GetLayers(snapshot, hmdPose);

  auto srv
    = snapshot.GetTexture<SHM::D3D11::Texture>()->GetD3D11ShaderResourceView();
  if (!srv) {
    dprint("Failed to get shared texture");
    return;
  }

  auto ctx = mDXR.mD3D11ImmediateContext.get();
  std::vector<size_t> activeLayers;

  const auto baseTint = snapshot.GetConfig().mTint;

  for (size_t layerIndex = 0; layerIndex < vrLayers.size(); ++layerIndex) {
    this->InitializeLayer(layerIndex);
    const auto [layer, renderParams] = vrLayers.at(layerIndex);
    auto& layerState = mLayers.at(layerIndex);

    if (renderParams.mCacheKey == layerState.mCacheKey) {
      continue;
    }
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
      srv,
      layer->mVR.mLocationOnTexture,
      {
        {0, 0},
        layer->mVR.mLocationOnTexture.mSize,
      },
      layerTint);
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

    layerState.mCacheKey = renderParams.mCacheKey;
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

    const auto enabled = snapshot.GetLayerConfig(i)->mVREnabled;
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
}

void SteamVRKneeboard::HideAllOverlays() {
  for (auto& layerState: mLayers) {
    if (layerState.mOverlay && layerState.mVisible) {
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
  const auto frameSleep
    = std::chrono::microseconds((1000 * 1000) / FramesPerSecond);

  dprint("Initializing OpenVR support");

  mGPUFlushEvent.close();
  mGPUFlushEvent = winrt::handle {CreateEventW(nullptr, false, false, nullptr)};

  while (!stopToken.stop_requested()) {
    if (
      IsSteamVRRunning() && vr::VR_IsHmdPresent() && this->InitializeOpenVR()) {
      this->Tick();
      if (this->mIVROverlay) {
        this->mIVROverlay->WaitFrameSync(frameSleep.count());
      } else {
        co_await resume_after(frameSleep, stopToken);
      }
      continue;
    }

    co_await resume_after(inactiveSleep, stopToken);
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
