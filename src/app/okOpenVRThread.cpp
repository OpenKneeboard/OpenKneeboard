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
#include "okOpenVRThread.h"

#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <TlHelp32.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <openvr.h>
#include <winrt/base.h>

#include <filesystem>

#pragma comment(lib, "D3D11.lib")

using namespace OpenKneeboard;

using namespace DirectX::SimpleMath;

class okOpenVRThread::Impl final {
 private:
  winrt::com_ptr<ID3D11Device1> mD3D;

 public:
  vr::IVRSystem* vrSystem = nullptr;
  vr::VROverlayHandle_t overlay {};
  SHM::Reader shm;

  ID3D11Device1* D3D();

  winrt::com_ptr<ID3D11Texture2D> openvrTexture;
  uint64_t sequenceNumber = 0;

  ~Impl() {
    if (!vrSystem) {
      return;
    }
    vr::VR_Shutdown();
  }
};

okOpenVRThread::okOpenVRThread() : p(std::make_unique<Impl>()) {
}

okOpenVRThread::~okOpenVRThread() {
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

void okOpenVRThread::Tick() {
  if (!p->vrSystem) {
    vr::EVRInitError err;
    p->vrSystem = vr::VR_Init(&err, vr::VRApplication_Background);
    if (!p->vrSystem) {
      return;
    }
    dprint("Initialized OpenVR");
  }

  if (!p->shm) {
    p->shm = SHM::Reader();
    if (!p->shm) {
      return;
    }
  }

  auto overlay = vr::VROverlay();
  if (!overlay) {
    return;
  }

  // reset before assign as we must call the old destructor before the new
  // constructor
#define CHECK(method, ...) \
  if (!overlay_check(overlay->method(__VA_ARGS__), #method)) { \
    p.reset(); \
    p = std::make_unique<Impl>(); \
    return; \
  }

  if (!p->overlay) {
    CHECK(
      CreateOverlay,
      "com.fredemmott.OpenKneeboard",
      "OpenKneeboard",
      &p->overlay);
    if (!p->overlay) {
      return;
    }

    dprintf("Created OpenVR overlay");
    static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
    CHECK(SetOverlayFlag, p->overlay, vr::VROverlayFlags_IsPremultiplied, true);
    CHECK(ShowOverlay, p->overlay);
  }

  vr::VREvent_t event;
  while (overlay->PollNextOverlayEvent(p->overlay, &event, sizeof(event))) {
    if (event.eventType == vr::VREvent_Quit) {
      dprint("OpenVR shutting down, detaching");
      p.reset();
      p = std::make_unique<Impl>();
      return;
    }
  }

  auto snapshot = p->shm.MaybeGet();
  if (!snapshot) {
    return;
  }

  const auto& config = *snapshot.GetConfig();

  bool zoomed = false;
  vr::TrackedDevicePose_t hmdPose {
    .bPoseIsValid = false,
    .bDeviceIsConnected = false,
  };
  p->vrSystem->GetDeviceToAbsoluteTrackingPose(
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
    zoomed = overlay->ComputeOverlayIntersection(p->overlay, &params, &results);
  }

  const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
  const auto& vrConf = config.vr;

  CHECK(
    SetOverlayWidthInMeters,
    p->overlay,
    vrConf.height * aspectRatio * (zoomed ? vrConf.zoomScale : 1.0f));

  if (p->sequenceNumber == snapshot.GetSequenceNumber()) {
    return;
  }
  p->sequenceNumber = snapshot.GetSequenceNumber();

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
    p->overlay,
    vr::TrackingUniverseStanding,
    reinterpret_cast<vr::HmdMatrix34_t*>(&transposed));

  // Using a Direct3D texture instead of SetOverlayRaw(), as SetOverlayRaw()
  // only works 200 times; SetOverlayTexture() keeps working 'forever'

  auto previousTexture = p->openvrTexture;
  if (previousTexture) {
    D3D11_TEXTURE2D_DESC desc;
    previousTexture->GetDesc(&desc);
    if (config.imageWidth != desc.Width || config.imageHeight != desc.Height) {
      p->openvrTexture = nullptr;
    }
  }

  auto d3d = p->D3D();
  if (!p->openvrTexture) {
    D3D11_TEXTURE2D_DESC desc {
      .Width = config.imageWidth,
      .Height = config.imageHeight,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
      .CPUAccessFlags = {},
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
    };
    d3d->CreateTexture2D(&desc, nullptr, p->openvrTexture.put());
    if (!p->openvrTexture) {
      dprint("Failed to create shared texture for OpenVR");
      return;
    }
  }

  // Copy the texture as for interoperability with other systems
  // (e.g. DirectX12) we use SHARED_NTHANDLE, but SteamVR doesn't
  // support that - so we need to use a second texture with different
  // sharing parameters.

  static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
  auto textureName = SHM::SharedTextureName();
  winrt::com_ptr<ID3D11Texture2D> openKneeboardTexture;
  d3d->OpenSharedResourceByName(
    textureName.c_str(),
    DXGI_SHARED_RESOURCE_READ,
    IID_PPV_ARGS(openKneeboardTexture.put()));

  D3D11_BOX sourceBox {
    .left = 0,
    .top = 0,
    .front = 0,
    .right = config.imageWidth,
    .bottom = config.imageHeight,
    .back = 1,
  };

  winrt::com_ptr<ID3D11DeviceContext> context;
  d3d->GetImmediateContext(context.put());
  auto mutex = openKneeboardTexture.as<IDXGIKeyedMutex>();
  const auto key = snapshot.GetTextureKey();
  if (mutex->AcquireSync(key, 10) != S_OK) {
    return;
  }
  context->CopySubresourceRegion(
    p->openvrTexture.get(),
    0,
    0,
    0,
    0,
    openKneeboardTexture.get(),
    0,
    &sourceBox);
  context->Flush();
  mutex->ReleaseSync(key);

  HANDLE handle = INVALID_HANDLE_VALUE;
  p->openvrTexture.as<IDXGIResource>()->GetSharedHandle(&handle);
  vr::Texture_t vrt {
    .handle = handle,
    .eType = vr::TextureType_DXGISharedHandle,
    .eColorSpace = vr::ColorSpace_Auto,
  };
  CHECK(SetOverlayTexture, p->overlay, &vrt);

#undef CHECK
}

ID3D11Device1* okOpenVRThread::Impl::D3D() {
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

wxThread::ExitCode okOpenVRThread::Entry() {
  if (!vr::VR_IsRuntimeInstalled()) {
    dprint("Shutting down OpenVR thread, no runtime installed.");
    return {0};
  }

  if (!p->D3D()) {
    dprint("Shutting down OpenVR thread, failed to get D3D11 device");
    return {0};
  }

  const auto inactiveSleepMS = 1000;
  const auto frameSleepMS = 1000 / 90;

  while (IsAlive()) {
    if (!IsSteamVRRunning()) {
      wxThread::This()->Sleep(inactiveSleepMS);
      continue;
    }

    this->Tick();
    wxThread::This()->Sleep(p->vrSystem ? frameSleepMS : inactiveSleepMS);
  }

  return {0};
}
