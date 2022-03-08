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
#include <OpenKneeboard/OpenVROverlay.h>
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

#pragma comment(lib, "D3D11.lib")

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

class OpenVROverlay::Impl final {
 private:
  winrt::com_ptr<ID3D11Device1> mD3D;

 public:
  vr::IVRSystem* mIVRSystem = nullptr;
  vr::IVROverlay* mIVROverlay = nullptr;
  vr::VROverlayHandle_t mOverlay {};
  bool mZoomed = false;
  SHM::Reader mSHM;

  ID3D11Device1* D3D();

  winrt::com_ptr<ID3D11Texture2D> mOpenVRTexture;
  uint64_t mSequenceNumber = 0;
  float mWidth = 0;
  float mZoomedWidth = 0;

  ~Impl() {
    if (!mIVRSystem) {
      return;
    }
    vr::VR_Shutdown();
  }
};

OpenVROverlay::OpenVROverlay() : p(std::make_unique<Impl>()) {
}

OpenVROverlay::~OpenVROverlay() {
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

void OpenVROverlay::Tick() {
  if (!p->mIVRSystem) {
    vr::EVRInitError err;
    p->mIVRSystem = vr::VR_Init(&err, vr::VRApplication_Background);
    if (!p->mIVRSystem) {
      return;
    }
    dprint("Initialized OpenVR");
  }

  if (!p->mSHM) {
    p->mSHM = SHM::Reader();
    if (!p->mSHM) {
      return;
    }
  }

  if (!p->mIVROverlay) {
    p->mIVROverlay = vr::VROverlay();
    if (!p->mIVROverlay) {
      return;
    }
  }

  // reset before assign as we must call the old destructor before the new
  // constructor
#define CHECK(method, ...) \
  if (!overlay_check(p->mIVROverlay->method(__VA_ARGS__), #method)) { \
    p.reset(); \
    p = std::make_unique<Impl>(); \
    return; \
  }

  if (!p->mOverlay) {
    CHECK(CreateOverlay, ProjectNameA, "OpenKneeboard", &p->mOverlay);
    if (!p->mOverlay) {
      return;
    }

    dprintf("Created OpenVR overlay");
    static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
    CHECK(
      SetOverlayFlag, p->mOverlay, vr::VROverlayFlags_IsPremultiplied, true);
    CHECK(ShowOverlay, p->mOverlay);
  }

  vr::VREvent_t event;
  while (
    p->mIVROverlay->PollNextOverlayEvent(p->mOverlay, &event, sizeof(event))) {
    if (event.eventType == vr::VREvent_Quit) {
      dprint("OpenVR shutting down, detaching");
      p.reset();
      p = std::make_unique<Impl>();
      return;
    }
  }

  vr::TrackedDevicePose_t hmdPose {
    .bPoseIsValid = false,
    .bDeviceIsConnected = false,
  };
  p->mIVRSystem->GetDeviceToAbsoluteTrackingPose(
    vr::TrackingUniverseStanding, 0, &hmdPose, 1);
  if (hmdPose.bDeviceIsConnected && hmdPose.bPoseIsValid) {
    const auto& f = hmdPose.mDeviceToAbsoluteTracking.m;
    // clang-format off
    Matrix m(
      f[0][0], f[1][0], f[2][0], 0,
      f[0][1], f[1][1], f[2][1], 0,
      f[0][2], f[1][2], f[2][2], 0,
      f[0][3], f[1][3], f[2][3], 1
    );
    // clang-format on

    auto translation = m.Translation();
    auto rotation = Vector3::TransformNormal(Vector3::Forward, m);

    vr::VROverlayIntersectionParams_t params {
      .vSource = {translation.x, translation.y, translation.z},
      .vDirection = {rotation.x, rotation.y, rotation.z},
      .eOrigin = vr::TrackingUniverseStanding,
    };

    vr::VROverlayIntersectionResults_t results;
    auto zoomed = p->mIVROverlay->ComputeOverlayIntersection(
      p->mOverlay, &params, &results);

    if (
      zoomed != p->mZoomed
      && p->mSequenceNumber == p->mSHM.GetSequenceNumber()) {
      CHECK(
        SetOverlayWidthInMeters,
        p->mOverlay,
        zoomed ? p->mZoomedWidth : p->mWidth);
    }
    p->mZoomed = zoomed;
  }

  if (p->mSHM.GetSequenceNumber() == p->mSequenceNumber) {
    return;
  }

  auto snapshot = p->mSHM.MaybeGet();
  if (!snapshot) {
    return;
  }

  const auto config = snapshot.GetConfig();
  const auto& vrConf = config.vr;

  const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
  p->mWidth = vrConf.height * aspectRatio;
  p->mZoomedWidth = p->mWidth * vrConf.zoomScale;

  CHECK(
    SetOverlayWidthInMeters,
    p->mOverlay,
    p->mZoomed ? p->mZoomedWidth : p->mWidth);

  const bool roomscale
    = vrConf.flags & VRConfig::Flags::PREFER_ROOMSCALE_POSITION;
  auto y = roomscale ? vrConf.floorY : vrConf.eyeY;
  auto universe
    = roomscale ? vr::TrackingUniverseStanding : vr::TrackingUniverseSeated;

  // clang-format off
  auto transform =
    Matrix::CreateRotationX(vrConf.rx)
    * Matrix::CreateRotationY(vrConf.ry)
    * Matrix::CreateRotationZ(vrConf.rz)
    * Matrix::CreateTranslation(vrConf.x, vrConf.floorY, vrConf.z);
  // clang-format on

  auto transposed = transform.Transpose();

  CHECK(
    SetOverlayTransformAbsolute,
    p->mOverlay,
    vr::TrackingUniverseStanding,
    reinterpret_cast<vr::HmdMatrix34_t*>(&transposed));

  auto d3d = p->D3D();
  if (!p->mOpenVRTexture) {
    D3D11_TEXTURE2D_DESC desc {
      .Width = TextureWidth,
      .Height = TextureHeight,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
      .CPUAccessFlags = {},
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
    };
    d3d->CreateTexture2D(&desc, nullptr, p->mOpenVRTexture.put());
    if (!p->mOpenVRTexture) {
      dprint("Failed to create shared texture for OpenVR");
      return;
    }
    HANDLE handle = INVALID_HANDLE_VALUE;
    p->mOpenVRTexture.as<IDXGIResource>()->GetSharedHandle(&handle);
    vr::Texture_t vrt {
      .handle = handle,
      .eType = vr::TextureType_DXGISharedHandle,
      .eColorSpace = vr::ColorSpace_Auto,
    };
    CHECK(SetOverlayTexture, p->mOverlay, &vrt);
  }

  // Copy the texture as for interoperability with other systems
  // (e.g. DirectX12) we use SHARED_NTHANDLE, but SteamVR doesn't
  // support that - so we need to use a second texture with different
  // sharing parameters.

  static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
  auto openKneeboardTexture = snapshot.GetSharedTexture(p->D3D());
  if (!openKneeboardTexture) {
    return;
  }
  winrt::com_ptr<ID3D11DeviceContext> context;

  d3d->GetImmediateContext(context.put());
  context->CopyResource(
    p->mOpenVRTexture.get(), openKneeboardTexture.GetTexture());
  context->Flush();

  vr::VRTextureBounds_t textureBounds {
    0.0f,
    0.0f,
    static_cast<float>(config.imageWidth) / TextureWidth,
    static_cast<float>(config.imageHeight) / TextureHeight,
  };

  CHECK(SetOverlayTextureBounds, p->mOverlay, &textureBounds);
  p->mSequenceNumber = snapshot.GetSequenceNumber();

#undef CHECK
}

ID3D11Device1* OpenVROverlay::Impl::D3D() {
  if (mD3D) {
    return mD3D.get();
  }

  D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  winrt::com_ptr<ID3D11Device> device;
  D3D11CreateDevice(
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
    nullptr);
  mD3D = device.as<ID3D11Device1>();
  return mD3D.get();
}

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

bool OpenVROverlay::Run(std::stop_token stopToken) {
  if (!vr::VR_IsRuntimeInstalled()) {
    dprint("Stopping OpenVR support, no runtime installed.");
    return true;
  }

  if (!p->D3D()) {
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

    this->Tick();
    std::this_thread::sleep_for(p->mIVRSystem ? frameSleep : inactiveSleep);
  }

  return true;
}

}// namespace OpenKneeboard
