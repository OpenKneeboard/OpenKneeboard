#include "okOpenVR.h"

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <openvr.h>

#include <Eigen/Geometry>

using namespace OpenKneeboard;

class okOpenVR::Impl final {
 public:
  vr::IVRSystem* System = nullptr;
  vr::VROverlayHandle_t Overlay {};
  bool Zoomed = false;

  ~Impl() {
    if (!System) {
      return;
    }
    vr::VR_Shutdown();
  }
};

okOpenVR::okOpenVR() : p(std::make_unique<Impl>()) {
}

okOpenVR::~okOpenVR() {
}

static void overlay_check(vr::EVROverlayError err) {
  if (err == vr::VROverlayError_None) {
    return;
  }

  dprintf(
    "OpenVR error: {}", vr::VROverlay()->GetOverlayErrorNameFromEnum(err));
}

void okOpenVR::Update(const SHM::Header& header, SHM::Pixel* pixels) {
  if (!p->System) {
    vr::EVRInitError err;
    p->System = vr::VR_Init(&err, vr::VRApplication_Background);
    if (!p->System) {
      return;
    }
  }

  auto overlay = vr::VROverlay();
  if (!p->Overlay) {
    overlay_check(
      overlay->CreateOverlay("OpenKneeboard", "OpenKneeboard", &p->Overlay));
    if (!p->Overlay) {
      return;
    }

    overlay_check(overlay->ShowOverlay(p->Overlay));
  }

  overlay_check(overlay->SetOverlayRaw(
    p->Overlay,
    reinterpret_cast<void*>(pixels),
    header.ImageWidth,
    header.ImageHeight,
    4));

  const auto overlayWidth
    = header.VirtualWidth * (p->Zoomed ? header.ZoomScale : 1.0f);

  overlay_check(overlay->SetOverlayWidthInMeters(p->Overlay, overlayWidth));

  Eigen::Transform<float, 3, Eigen::AffineCompact, Eigen::RowMajor> t;
  t = Eigen::Translation3f(header.x, header.floorY, header.z)
    // Negate as we use clockwise angles for human-friendliness
    * Eigen::AngleAxisf(-header.rx, Eigen::Vector3f::UnitX())
    * Eigen::AngleAxisf(-header.ry, Eigen::Vector3f::UnitY())
    * Eigen::AngleAxisf(-header.rz, Eigen::Vector3f::UnitZ());

  overlay_check(overlay->SetOverlayTransformAbsolute(
    p->Overlay,
    vr::TrackingUniverseStanding,
    reinterpret_cast<const vr::HmdMatrix34_t*>(t.data())));
}
