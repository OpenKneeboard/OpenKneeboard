#include "OculusKneeboard.h"

#pragma warning(push)
// lossy conversions (double -> T)
#pragma warning(disable : 4244)
#include <Extras/OVR_Math.h>
#pragma warning(pop)

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

static bool poseIntersectsWithRect(
  const ovrPosef& pose,
  const ovrPosef& rectCenter,
  const ovrVector2f& rectSize) {
  const auto rayOrigin = OVR::Vector3f(pose.Position);
  const auto rayNormal = OVR::Quatf(pose.Orientation) * OVR::Vector3f(0, 0, -1);
  const auto planeQuat = OVR::Quatf(rectCenter.Orientation);
  const auto planeOrigin = OVR::Vector3f(rectCenter.Position);
  const auto planeNormal = planeQuat * OVR::Vector3f(0, 0, 1);

  // Does the ray intersect the infinite plane?
  if (rayNormal.Dot(planeNormal) >= 0) {
    return false;
  }

  // Where does it intersect the infinite plane?
  const auto rayOriginToPlaneOrigin = planeOrigin - rayOrigin;
  const auto rayLength
    = (planeOrigin - rayOrigin).Dot(planeNormal) / rayNormal.Dot(planeNormal);
  const auto worldPoint = rayOrigin + (rayNormal * rayLength);

  /// Finally: is the intersection point within the rectangle?
  const auto point = worldPoint - planeOrigin;

  const auto xVector = planeQuat * OVR::Vector3f(1, 0, 0);
  const auto x = point.Dot(xVector);

  if (abs(x) > rectSize.x / 2) {
    return false;
  }

  const auto yVector = planeQuat * OVR::Vector3f(0, 1, 0);
  const auto y = point.Dot(yVector);
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

ovrResult OculusKneeboard::onEndFrame(
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
  kneeboardLayer.Header.Flags = {};
  kneeboardLayer.ColorTexture = swapChain;
  kneeboardLayer.QuadPoseCenter.Position
    = {.x = config.x, .y = config.floorY, .z = config.z};

  if ((config.Flags & SHM::Flags::HEADLOCKED)) {
    kneeboardLayer.Header.Flags |= ovrLayerFlag_HeadLocked;
  } else if (
    real_ovr_GetTrackingOriginType(session) == ovrTrackingOrigin_EyeLevel) {
    kneeboardLayer.QuadPoseCenter.Position.y = config.eyeY;
  }

  OVR::Quatf orientation;
  orientation *= OVR::Quatf(OVR::Axis::Axis_X, config.rx);
  orientation *= OVR::Quatf(OVR::Axis::Axis_Y, config.ry);
  orientation *= OVR::Quatf(OVR::Axis::Axis_Z, config.rz);
  kneeboardLayer.QuadPoseCenter.Orientation = orientation;

  kneeboardLayer.Viewport.Pos = {.x = 0, .y = 0};
  kneeboardLayer.Viewport.Size
    = {.w = config.ImageWidth, .h = config.ImageHeight};

  const ovrVector2f normalSize(config.VirtualWidth, config.VirtualHeight);
  const ovrVector2f zoomedSize(
    config.VirtualWidth * config.ZoomScale,
    config.VirtualHeight * config.ZoomScale);

  auto predictedTime = real_ovr_GetPredictedDisplayTime(session, frameIndex);
  auto state = real_ovr_GetTrackingState(session, predictedTime, false);

  mZoomed = poseIntersectsWithRect(
    state.HeadPose.ThePose,
    kneeboardLayer.QuadPoseCenter,
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
