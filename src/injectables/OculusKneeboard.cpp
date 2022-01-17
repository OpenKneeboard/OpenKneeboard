#include "OculusKneeboard.h"

#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/dprint.h>

// clang-format off
#include <windows.h>
#include <detours.h>
// clang-format on

#include "OVRProxy.h"

using namespace OpenKneeboard;
using namespace DirectX;
using namespace DirectX::SimpleMath;

static bool poseIntersectsWithRect(
  const ovrPosef& pose,
  const Vector3& rectCenter,
  const Quaternion& rectQuat,
  const ovrVector2f& rectSize) {
  const Quaternion rayQuat(
    pose.Orientation.x,
    pose.Orientation.y,
    pose.Orientation.z,
    pose.Orientation.w);
  const Vector3 rayOrigin(pose.Position.x, pose.Position.y, pose.Position.z);
  const Vector3 rayNormal(Vector3::Transform(Vector3::Forward, rayQuat));
  const Ray ray(rayOrigin, rayNormal);

  const Plane plane(
    rectCenter, Vector3::Transform(Vector3::Backward, rectQuat));

  // Does the ray intersect the infinite plane?
  float rayLength = 0;
  if (!ray.Intersects(plane, rayLength)) {
    return false;
  }

  // Where does it intersect the infinite plane?
  const auto worldPoint = rayOrigin + (rayNormal * rayLength);

  // Is that point within the rectangle?
  const auto point = worldPoint - rectCenter;

  const auto x = point.Dot(Vector3::Transform(Vector3::UnitX, rectQuat));
  if (abs(x) > rectSize.x / 2) {
    return false;
  }

  const auto y = point.Dot(Vector3::Transform(Vector3::UnitY, rectQuat));
  if (abs(y) > rectSize.y / 2) {
    return false;
  }

  return true;
}

namespace OpenKneeboard {

struct OculusKneeboard::Impl {
  SHM::Reader shm;
  bool zoomed = false;
  std::unique_ptr<OVRProxy> ovr;
  std::unique_ptr<OculusEndFrameHook> endFrameHook;
};

OculusKneeboard::OculusKneeboard() : p(std::make_unique<Impl>()) {
  dprintf("{} {:#018x}", __FUNCTION__, (uint64_t)this);
}

OculusKneeboard::~OculusKneeboard() {
  this->UninstallHook();
}

void OculusKneeboard::UninstallHook() {
  if (p->endFrameHook) {
    p->endFrameHook->UninstallHook();
  }
}

void OculusKneeboard::InitWithVTable() {
  if (p->endFrameHook) {
    return;
  }
  p->endFrameHook = OculusEndFrameHook::make_unique({
    .onHookInstalled
    = std::bind_front(&OculusKneeboard::OnOVREndFrameHookInstalled, this),
    .onEndFrame = std::bind_front(&OculusKneeboard::OnOVREndFrame, this),
  });
}

void OculusKneeboard::OnOVREndFrameHookInstalled() {
  p->ovr = std::make_unique<OVRProxy>();
}

ovrResult OculusKneeboard::OnOVREndFrame(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount,
  const decltype(&ovr_EndFrame)& next) {
  auto snapshot = p->shm.MaybeGet();
  if (!snapshot) {
    return next(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
  }

  const auto& config = *snapshot.GetConfig();
  auto swapChain = GetSwapChain(session, config);
  if (!(swapChain && Render(session, swapChain, snapshot))) {
    return next(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
  }

  ovrLayerQuad kneeboardLayer = {};
  kneeboardLayer.Header.Type = ovrLayerType_Quad;
  // TODO: set ovrLayerFlag_TextureOriginAtBottomLeft for OpenGL?
  kneeboardLayer.Header.Flags = {ovrLayerFlag_HighQuality};
  kneeboardLayer.ColorTexture = swapChain;

  const auto& vr = config.vr;
  Vector3 position(vr.x, vr.floorY, vr.z);
  if ((config.vr.flags & SHM::VRConfig::Flags::HEADLOCKED)) {
    kneeboardLayer.Header.Flags |= ovrLayerFlag_HeadLocked;
  } else if (
    p->ovr->ovr_GetTrackingOriginType(session) == ovrTrackingOrigin_EyeLevel) {
    position.y = vr.eyeY;
  }
  kneeboardLayer.QuadPoseCenter.Position = {position.x, position.y, position.z};

  // clang-format off
  auto orientation =
    Quaternion::CreateFromAxisAngle(Vector3::UnitX, vr.rx)
    * Quaternion::CreateFromAxisAngle(Vector3::UnitY, vr.ry)
    * Quaternion::CreateFromAxisAngle(Vector3::UnitZ, vr.rz);
  // clang-format on

  kneeboardLayer.QuadPoseCenter.Orientation
    = {orientation.x, orientation.y, orientation.z, orientation.w};

  kneeboardLayer.Viewport.Pos = {.x = 0, .y = 0};
  kneeboardLayer.Viewport.Size
    = {.w = config.imageWidth, .h = config.imageHeight};

  const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
  const auto virtualHeight = vr.height;
  const auto virtualWidth = aspectRatio * vr.height;

  const ovrVector2f normalSize(virtualWidth, virtualHeight);
  const ovrVector2f zoomedSize(
    virtualWidth * vr.zoomScale, virtualHeight * vr.zoomScale);

  auto predictedTime = p->ovr->ovr_GetPredictedDisplayTime(session, frameIndex);
  auto state = p->ovr->ovr_GetTrackingState(session, predictedTime, false);

  p->zoomed = poseIntersectsWithRect(
    state.HeadPose.ThePose,
    position,
    orientation,
    p->zoomed ? zoomedSize : normalSize);
  kneeboardLayer.QuadSize = p->zoomed ? zoomedSize : normalSize;

  std::vector<const ovrLayerHeader*> newLayers;
  if (layerCount == 0) {
    newLayers.push_back(&kneeboardLayer.Header);
  } else if (layerCount < ovrMaxLayerCount) {
    newLayers = {&layerPtrList[0], &layerPtrList[layerCount]};
    newLayers.push_back(&kneeboardLayer.Header);
  } else {
    for (unsigned int i = 0; i < layerCount; ++i) {
      if (layerPtrList[i]) {
        newLayers.push_back(layerPtrList[i]);
      }
    }

    if (newLayers.size() < ovrMaxLayerCount) {
      newLayers.push_back(&kneeboardLayer.Header);
    } else {
      dprintf("Already at ovrMaxLayerCount without adding our layer");
    }
  }

  std::vector<ovrLayerEyeFov> withoutDepthInformation;
  if ((config.vr.flags & SHM::VRConfig::Flags::DISCARD_DEPTH_INFORMATION)) {
    for (auto i = 0; i < newLayers.size(); ++i) {
      auto layer = newLayers.at(i);
      if (layer->Type != ovrLayerType_EyeFovDepth) {
        continue;
      }

      withoutDepthInformation.push_back({});
      auto& newLayer
        = withoutDepthInformation[withoutDepthInformation.size() - 1];
      memcpy(&newLayer, layer, sizeof(ovrLayerEyeFov));
      newLayer.Header.Type = ovrLayerType_EyeFov;

      newLayers[i] = &newLayer.Header;
    }
  }

  return next(
    session,
    frameIndex,
    viewScaleDesc,
    newLayers.data(),
    static_cast<unsigned int>(newLayers.size()));
}

}// namespace OpenKneeboard
