#include "okOpenVRThread.h"

#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <openvr.h>
#include <winrt/base.h>

#pragma comment(lib, "D3D11.lib")

using namespace OpenKneeboard;

using namespace DirectX;
using namespace DirectX::SimpleMath;

class okOpenVRThread::Impl final {
 public:
  vr::IVRSystem* VRSystem = nullptr;
  vr::VROverlayHandle_t Overlay {};
  SHM::Reader SHM;
  winrt::com_ptr<ID3D11Device> D3D;
  winrt::com_ptr<ID3D11Texture2D> Texture;
  winrt::com_ptr<ID3D11RenderTargetView> RenderTargetView;
  uint64_t SequenceNumber = 0;

  ~Impl() {
    if (!VRSystem) {
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
  if (!p->VRSystem) {
    vr::EVRInitError err;
    p->VRSystem = vr::VR_Init(&err, vr::VRApplication_Background);
    if (!p->VRSystem) {
      return;
    }
    dprint("Initialized OpenVR");
  }

  if (!p->SHM) {
    p->SHM = SHM::Reader();
    if (!p->SHM) {
      return;
    }
  }

  auto overlay = vr::VROverlay();
  if (!overlay) {
    return;
  }

#define CHECK(method, ...) \
  if (!overlay_check(overlay->method(__VA_ARGS__), #method)) { \
    p = std::make_unique<Impl>(); \
    return; \
  }

  if (!p->Overlay) {
    CHECK(
      CreateOverlay,
      "com.fredemmott.OpenKneeboard",
      "OpenKneeboard",
      &p->Overlay);
    if (!p->Overlay) {
      return;
    }

    dprintf("Created OpenVR overlay");
    CHECK(ShowOverlay, p->Overlay);
  }

  auto snapshot = p->SHM.MaybeGet();
  if (!snapshot) {
    return;
  }

  const auto& header = *snapshot.GetHeader();

  bool zoomed = false;
  vr::TrackedDevicePose_t hmdPose {
    .bPoseIsValid = false,
    .bDeviceIsConnected = false,
  };
  p->VRSystem->GetDeviceToAbsoluteTrackingPose(
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
    zoomed = overlay->ComputeOverlayIntersection(p->Overlay, &params, &results);
  }

  const auto aspectRatio = float(header.ImageWidth) / header.ImageHeight;

  CHECK(
    SetOverlayWidthInMeters,
    p->Overlay,
    header.VirtualHeight * aspectRatio * (zoomed ? header.ZoomScale : 1.0f));

  if (p->SequenceNumber == header.SequenceNumber) {
    return;
  }
  p->SequenceNumber = header.SequenceNumber;

  // clang-format off
  auto transform =
    Matrix::CreateRotationX(header.rx)
    * Matrix::CreateRotationY(header.ry)
    * Matrix::CreateRotationZ(header.rz)
    * Matrix::CreateTranslation(header.x, header.floorY, header.z);
  // clang-format on

  auto transposed = transform.Transpose();

  CHECK(
    SetOverlayTransformAbsolute,
    p->Overlay,
    vr::TrackingUniverseStanding,
    reinterpret_cast<vr::HmdMatrix34_t*>(&transposed));

  // Using a Direct3D texture instead of SetOverlayRaw(), as SetOverlayRaw()
  // only works 200 times; SetOverlayTexture() keeps working 'forever'

  auto previousTexture = p->Texture;
  if (previousTexture) {
    D3D11_TEXTURE2D_DESC desc;
    previousTexture->GetDesc(&desc);
    if (header.ImageWidth != desc.Width || header.ImageHeight != desc.Height) {
      p->Texture = nullptr;
    }
  }

  if (!p->Texture) {
    D3D11_TEXTURE2D_DESC desc {
      .Width = header.ImageWidth,
      .Height = header.ImageHeight,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = {},
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
    };
    p->D3D->CreateTexture2D(&desc, nullptr, p->Texture.put());
    if (!p->Texture) {
      dprint("Failed to create texture for OpenVR");
      return;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MipSlice = 0},
    };
    p->RenderTargetView = nullptr;
    p->D3D->CreateRenderTargetView(
      p->Texture.get(), &rtvd, p->RenderTargetView.put());
    if (!p->RenderTargetView) {
      dprint("Failed to create RenderTargetView for OpenVR");
      return;
    }
  }

  static_assert(sizeof(SHM::Pixel) == 4, "Expecting B8G8R8A8 for DirectX");
  static_assert(offsetof(SHM::Pixel, b) == 0, "Expected blue to be first byte");
  static_assert(offsetof(SHM::Pixel, a) == 3, "Expected alpha to be last byte");

  {
    D3D11_BOX box {
      .left = 0,
      .top = 0,
      .front = 0,
      .right = header.ImageWidth,
      .bottom = header.ImageHeight,
      .back = 1,
    };

    winrt::com_ptr<ID3D11DeviceContext> context;
    p->D3D->GetImmediateContext(context.put());

    context->UpdateSubresource(
      p->Texture.get(),
      0,
      &box,
      snapshot.GetPixels(),
      header.ImageWidth * sizeof(SHM::Pixel),
      0);
    vr::Texture_t vrt {
      .handle = p->Texture.get(),
      .eType = vr::TextureType_DirectX,
      .eColorSpace = vr::ColorSpace_Auto,
    };
    CHECK(SetOverlayTexture, p->Overlay, &vrt);
    context->Flush();
  }

#undef CHECK
}

wxThread::ExitCode okOpenVRThread::Entry() {
  if (!vr::VR_IsRuntimeInstalled()) {
    dprint("Shutting down OpenVR thread, no runtime installed.");
    return {0};
  }

  D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
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
    p->D3D.put(),
    nullptr,
    nullptr);

  if (!p->D3D) {
    dprint("Shutting down OpenVR thread, failed to get D3D11 device");
    return {0};
  }

  const auto inactiveSleepMS = 1000;
  const auto frameSleepMS = 1000 / 90;

  while (IsAlive()) {
    if (!vr::VR_IsHmdPresent()) {
      wxThread::This()->Sleep(inactiveSleepMS);
      continue;
    }

    this->Tick();
    wxThread::This()->Sleep(p->VRSystem ? frameSleepMS : inactiveSleepMS);
  }

  return {0};
}
