#include "OculusKneeboard.h"

#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/dprint.h>

// clang-format off
#include <windows.h>
#include <detours.h>
// clang-format on

#define LIBOVR_FUNCS \
  IT(ovr_GetTrackingState) \
  IT(ovr_GetPredictedDisplayTime) \
  IT(ovr_GetTrackingOriginType)

#define IT(x) static decltype(&x) real_##x = nullptr;
LIBOVR_FUNCS
#undef IT

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

  const Plane plane(rectCenter, Vector3::Transform(Vector3::Backward, rectQuat));

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

OculusKneeboard::OculusKneeboard() {
#define IT(x) \
  real_##x = reinterpret_cast<decltype(&x)>( \
    DetourFindFunction("LibOVRRT64_1.dll", #x));
  LIBOVR_FUNCS
#undef IT
}

OculusKneeboard::~OculusKneeboard() {
}

void OculusKneeboard::Unhook() {
  OculusFrameHook::Unhook();
}

ovrResult OculusKneeboard::OnEndFrame(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount,
  decltype(&ovr_EndFrame) next) {
  auto snapshot = mSHM.MaybeGet();
  if (!snapshot) {
    return next(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
  }

  const auto& config = *snapshot.GetHeader();
  auto swapChain = GetSwapChain(session, config);
  if (!(swapChain && Render(session, swapChain, snapshot))) {
    return next(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
  }

  ovrLayerQuad kneeboardLayer = {};
  kneeboardLayer.Header.Type = ovrLayerType_Quad;
  // TODO: set ovrLayerFlag_TextureOriginAtBottomLeft for OpenGL?
  kneeboardLayer.Header.Flags = {ovrLayerFlag_HighQuality};
  kneeboardLayer.ColorTexture = swapChain;

  Vector3 position(config.x, config.floorY, config.z);
  if ((config.Flags & SHM::Flags::HEADLOCKED)) {
    kneeboardLayer.Header.Flags |= ovrLayerFlag_HeadLocked;
  } else if (
    real_ovr_GetTrackingOriginType(session) == ovrTrackingOrigin_EyeLevel) {
    position.y = config.eyeY;
  }
  kneeboardLayer.QuadPoseCenter.Position = {position.x, position.y, position.z};

  // clang-format off
  auto orientation =
    Quaternion::CreateFromAxisAngle(Vector3::UnitX, config.rx)
    * Quaternion::CreateFromAxisAngle(Vector3::UnitY, config.ry)
    * Quaternion::CreateFromAxisAngle(Vector3::UnitZ, config.rz);
  // clang-format on

  kneeboardLayer.QuadPoseCenter.Orientation
    = {orientation.x, orientation.y, orientation.z, orientation.w};

  kneeboardLayer.Viewport.Pos = {.x = 0, .y = 0};
  kneeboardLayer.Viewport.Size
    = {.w = config.ImageWidth, .h = config.ImageHeight};

  const auto aspectRatio = float(config.ImageWidth) / config.ImageHeight;
  const auto virtualWidth = aspectRatio * config.VirtualHeight;

  const ovrVector2f normalSize(virtualWidth, config.VirtualHeight);
  const ovrVector2f zoomedSize(
    virtualWidth * config.ZoomScale,
    config.VirtualHeight * config.ZoomScale);

  auto predictedTime = real_ovr_GetPredictedDisplayTime(session, frameIndex);
  auto state = real_ovr_GetTrackingState(session, predictedTime, false);

  mZoomed = poseIntersectsWithRect(
    state.HeadPose.ThePose,
    position,
    orientation,
    mZoomed ? zoomedSize : normalSize);
  kneeboardLayer.QuadSize = mZoomed ? zoomedSize : normalSize;

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
  if ((config.Flags & SHM::Flags::DISCARD_DEPTH_INFORMATION)) {
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
