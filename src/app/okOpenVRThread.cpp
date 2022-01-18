#include "okOpenVRThread.h"

#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <TlHelp32.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <openvr.h>
#include <winrt/base.h>

#include <filesystem>

#pragma comment(lib, "D3D11.lib")

using namespace OpenKneeboard;

using namespace DirectX;
using namespace DirectX::SimpleMath;

class okOpenVRThread::Impl final {
 private:
  winrt::com_ptr<ID3D11Device> mD3D;

 public:
  vr::IVRSystem* vrSystem = nullptr;
  vr::VROverlayHandle_t overlay {};
  SHM::Reader shm;

  ID3D11Device* D3D();

  winrt::com_ptr<ID3D11Texture2D> texture;
  winrt::com_ptr<ID3D11RenderTargetView> renderTargetView;
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

  auto previousTexture = p->texture;
  if (previousTexture) {
    D3D11_TEXTURE2D_DESC desc;
    previousTexture->GetDesc(&desc);
    if (config.imageWidth != desc.Width || config.imageHeight != desc.Height) {
      p->texture = nullptr;
    }
  }

  auto d3d = p->D3D();
  if (!p->texture) {
    D3D11_TEXTURE2D_DESC desc {
      .Width = config.imageWidth,
      .Height = config.imageHeight,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = {},
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
    };
    d3d->CreateTexture2D(&desc, nullptr, p->texture.put());
    if (!p->texture) {
      dprint("Failed to create texture for OpenVR");
      return;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MipSlice = 0},
    };
    p->renderTargetView = nullptr;
    d3d->CreateRenderTargetView(
      p->texture.get(), &rtvd, p->renderTargetView.put());
    if (!p->renderTargetView) {
      dprint("Failed to create RenderTargetView for OpenVR");
      return;
    }
  }

  static_assert(SHM::Pixel::IS_PREMULTIPLIED_B8G8R8A8);

  {
    D3D11_BOX box {
      .left = 0,
      .top = 0,
      .front = 0,
      .right = config.imageWidth,
      .bottom = config.imageHeight,
      .back = 1,
    };

    winrt::com_ptr<ID3D11DeviceContext> context;
    d3d->GetImmediateContext(context.put());

    context->UpdateSubresource(
      p->texture.get(),
      0,
      &box,
      snapshot.GetPixels(),
      config.imageWidth * sizeof(SHM::Pixel),
      0);
    vr::Texture_t vrt {
      .handle = p->texture.get(),
      .eType = vr::TextureType_DirectX,
      .eColorSpace = vr::ColorSpace_Auto,
    };
    CHECK(SetOverlayTexture, p->overlay, &vrt);
    context->Flush();
  }

#undef CHECK
}

ID3D11Device* okOpenVRThread::Impl::D3D() {
  if (mD3D) {
    return mD3D.get();
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
    mD3D.put(),
    nullptr,
    nullptr);
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
