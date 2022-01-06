#include "okOpenVRThread.h"

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <openvr.h>

#include <Eigen/Geometry>

using namespace OpenKneeboard;

class okOpenVRThread::Impl final {
 public:
  vr::IVRSystem* VRSystem = nullptr;
  vr::VROverlayHandle_t Overlay {};
  bool Zoomed = false;
  SHM::Reader SHM;

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

  auto snapshot = p->SHM.MaybeGet();
  if (!snapshot) {
    return;
  }

  auto overlay = vr::VROverlay();
  if (!overlay) {
    return;
  }

  // Should probably call VR_Shutdown() here, but sometimes vrserver
  // crashes, leading to this process cleanly exiting with 0, so
  // just assume the call failed because VRServer went away and
  // the handles we have are invalid
#define CHECK(method, ...) \
  if (!overlay_check(overlay->method(__VA_ARGS__), #method)) { \
    p->VRSystem = nullptr; \
    p->Overlay = {}; \
    return; \
  }

  if (!p->Overlay) {
    CHECK(CreateOverlay, "OpenKneeboard", "OpenKneeboard", &p->Overlay);
    if (!p->Overlay) {
      return;
    }

    dprintf("Created OpenVR overlay");

    CHECK(ShowOverlay, p->Overlay);
  }

  const auto& header = *snapshot.GetHeader();

  CHECK(
    SetOverlayRaw,
    p->Overlay,
    const_cast<void*>(reinterpret_cast<const void*>(snapshot.GetPixels())),
    header.ImageWidth,
    header.ImageHeight,
    4);

  CHECK(
    SetOverlayWidthInMeters,
    p->Overlay,
    header.VirtualWidth * (p->Zoomed ? header.ZoomScale : 1.0f));

  Eigen::Transform<float, 3, Eigen::AffineCompact, Eigen::RowMajor> t;
  t = Eigen::Translation3f(header.x, header.floorY, header.z)
    * Eigen::AngleAxisf(header.rx, Eigen::Vector3f::UnitX())
    * Eigen::AngleAxisf(header.ry, Eigen::Vector3f::UnitY())
    * Eigen::AngleAxisf(header.rz, Eigen::Vector3f::UnitZ());

  CHECK(
    SetOverlayTransformAbsolute,
    p->Overlay,
    vr::TrackingUniverseStanding,
    reinterpret_cast<const vr::HmdMatrix34_t*>(t.data()));
#undef CHECK
}

wxThread::ExitCode okOpenVRThread::Entry() {
  if (!vr::VR_IsRuntimeInstalled()) {
    dprint("Shutting down OpenVR thread, no runtime installed.");
    return {0};
  }

  const auto inactiveSleepMS = 1000;
  const auto frameSleepMS = 1000 / 30;

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
