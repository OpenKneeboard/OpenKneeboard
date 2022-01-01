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
  IT(ovr_GetTrackingOriginType)

#define IT(x) static decltype(&x) real_##x = nullptr;
LIBOVR_FUNCS
#undef IT

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
  kneeboardLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
  kneeboardLayer.ColorTexture = swapChain;
  kneeboardLayer.QuadPoseCenter.Position
    = {.x = config.x, .y = config.floorY, .z = config.z};

  if ((config.Flags & OpenKneeboard::Flags::HEADLOCKED)) {
    kneeboardLayer.Header.Flags |= ovrLayerFlag_HeadLocked;
  } else if (real_ovr_GetTrackingOriginType(session) == ovrTrackingOrigin_EyeLevel) {
    kneeboardLayer.QuadPoseCenter.Position.y = config.eyeY;
  }

  OVR::Quatf orientation;
  orientation *= OVR::Quatf(OVR::Axis::Axis_X, config.rx);
  orientation *= OVR::Quatf(OVR::Axis::Axis_Y, config.ry);
  orientation *= OVR::Quatf(OVR::Axis::Axis_Z, config.rz);
  kneeboardLayer.QuadPoseCenter.Orientation = orientation;
  kneeboardLayer.QuadSize
    = {.x = config.VirtualWidth, .y = config.VirtualHeight};
  kneeboardLayer.Viewport.Pos = {.x = 0, .y = 0};
  kneeboardLayer.Viewport.Size
    = {.w = config.ImageWidth, .h = config.ImageHeight};

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
  if ((config.Flags & OpenKneeboard::Flags::DISCARD_DEPTH_INFORMATION)) {
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
