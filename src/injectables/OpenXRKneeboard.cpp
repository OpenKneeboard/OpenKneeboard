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
#include <OpenKneeboard/RayIntersectsRect.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <d3d11.h>
#include <loader_interfaces.h>
#include <openxr/openxr.h>
#include <shims/winrt.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

static constexpr XrPosef XR_POSEF_IDENTITY {
  .orientation = {0.0f, 0.0f, 0.0f, 1.0f},
  .position = {0.0f, 0.0f, 0.0f},
};

const std::string_view OpenXRLayerName {"XR_APILAYER_NOVENDOR_OpenKneeboard"};

#define NEEDED_OPENXR_FUNCS \
  IT(xrDestroyInstance) \
  IT(xrCreateSession) \
  IT(xrDestroySession) \
  IT(xrEndFrame) \
  IT(xrCreateSwapchain) \
  IT(xrEnumerateSwapchainImages) \
  IT(xrAcquireSwapchainImage) \
  IT(xrReleaseSwapchainImage) \
  IT(xrWaitSwapchainImage) \
  IT(xrDestroySwapchain) \
  IT(xrCreateReferenceSpace) \
  IT(xrDestroySpace) \
  IT(xrLocateSpace)

static struct {
  PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr {nullptr};
#define IT(x) PFN_##x x {nullptr};
  NEEDED_OPENXR_FUNCS
#undef IT
} gNext;

class OpenXRKneeboard {
 public:
  OpenXRKneeboard() = delete;
  OpenXRKneeboard(XrSession);
  virtual ~OpenXRKneeboard();
  virtual XrResult xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo)
    = 0;

 protected:
  XrPosef GetHMDPose(XrTime displayTime);
  XrExtent2Df
  GetSize(const SHM::Config&, const Matrix& kneeboardPose, XrTime displayTime);
  void Recenter(XrTime displayTime);

  bool mZoomed = false;
  XrSpace mLocalSpace = nullptr;
  XrSpace mViewSpace = nullptr;

  int64_t mRecenterCount = 0;
  Matrix mRecenter = Matrix::Identity;
};

class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(XrSession, ID3D11Device*);
  ~OpenXRD3D11Kneeboard();

  virtual XrResult xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo) override;

 private:
  void Render(const SHM::Snapshot&);

  SHM::Reader mSHM;
  ID3D11Device* mDevice = nullptr;
  XrSwapchain mSwapchain = nullptr;
  uint64_t mSequenceNumber = ~(0ui64);

  std::vector<winrt::com_ptr<ID3D11Texture2D>> mTextures;
};

OpenXRKneeboard::OpenXRKneeboard(XrSession session) {
  dprintf("{}", __FUNCTION__);
  XrReferenceSpaceCreateInfo referenceSpace {
    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
    .next = nullptr,
    .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
    .poseInReferenceSpace = XR_POSEF_IDENTITY,
  };

  XrResult nextResult
    = gNext.xrCreateReferenceSpace(session, &referenceSpace, &mLocalSpace);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to create LOCAL reference space: {}", nextResult);
    return;
  }

  referenceSpace.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  nextResult
    = gNext.xrCreateReferenceSpace(session, &referenceSpace, &mViewSpace);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to create VIEW reference space: {}", nextResult);
    return;
  }
}

OpenXRKneeboard::~OpenXRKneeboard() {
  if (mLocalSpace) {
    gNext.xrDestroySpace(mLocalSpace);
  }
  if (mViewSpace) {
    gNext.xrDestroySpace(mViewSpace);
  }
}

XrPosef OpenXRKneeboard::GetHMDPose(XrTime displayTime) {
  static XrPosef sCache = XR_POSEF_IDENTITY;
  static XrTime sCacheKey {};
  if (displayTime == sCacheKey) {
    return sCache;
  }

  XrSpaceLocation location {.type = XR_TYPE_SPACE_LOCATION};
  if (
    gNext.xrLocateSpace(mViewSpace, mLocalSpace, displayTime, &location)
    != XR_SUCCESS) {
    return XR_POSEF_IDENTITY;
  }

  const auto desiredFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
    | XR_SPACE_LOCATION_POSITION_VALID_BIT;
  if ((location.locationFlags & desiredFlags) != desiredFlags) {
    return XR_POSEF_IDENTITY;
  }

  sCache = location.pose;
  sCacheKey = displayTime;
  return sCache;
}

XrExtent2Df OpenXRKneeboard::GetSize(
  const SHM::Config& config,
  const Matrix& kneeboardPose,
  XrTime displayTime) {
  const auto& vr = config.vr;

  const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
  const auto virtualHeight = vr.height;
  const auto virtualWidth = aspectRatio * vr.height;

  XrExtent2Df normalSize {virtualWidth, virtualHeight};
  XrExtent2Df zoomedSize {
    virtualWidth * vr.zoomScale, virtualHeight * vr.zoomScale};

  if (vr.flags & VRConfig::Flags::FORCE_ZOOM) {
    mZoomed = true;
    return zoomedSize;
  }
  if (!(vr.flags & VRConfig::Flags::GAZE_ZOOM)) {
    mZoomed = false;
    return normalSize;
  }
  if (
    vr.zoomScale < 1.1 || vr.gazeTargetHorizontalScale < 0.1
    || vr.gazeTargetVerticalScale < 0.1) {
    mZoomed = false;
    return normalSize;
  }

  const auto hmdPose = this->GetHMDPose(displayTime);

  const auto& pos = hmdPose.position;
  const auto& quat = hmdPose.orientation;

  mZoomed = RayIntersectsRect(
    {pos.x, pos.y, pos.z},
    {quat.x, quat.y, quat.z, quat.w},
    kneeboardPose.Translation(),
    Quaternion::CreateFromRotationMatrix(kneeboardPose),
    Vector2 {
      virtualWidth * vr.gazeTargetHorizontalScale
        * (mZoomed ? vr.zoomScale : 1.0f),
      virtualHeight * vr.gazeTargetVerticalScale
        * (mZoomed ? vr.zoomScale : 1.0f),
    });
  return mZoomed ? zoomedSize : normalSize;
}

void OpenXRKneeboard::Recenter(XrTime displayTime) {
  auto hmd = this->GetHMDPose(displayTime);

  auto& pos = hmd.position;
  pos.y = 0;// don't adjust floor level
  Quaternion quat {
    hmd.orientation.x,
    hmd.orientation.y,
    hmd.orientation.z,
    hmd.orientation.w,
  };

  // We're only going to respect ry (yaw) as we want the new
  // center to remain gravity-aligned

  // clang-format off
  mRecenter =
    Matrix::CreateRotationY(quat.ToEuler().y) 
    * Matrix::CreateTranslation({pos.x, pos.y, pos.z});
  // clang-format on
}

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrSession session,
  ID3D11Device* device)
  : OpenXRKneeboard(session), mDevice(device) {
  dprintf("{}", __FUNCTION__);
  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    .sampleCount = 1,
    .width = TextureWidth,
    .height = TextureHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };
  auto nextResult
    = gNext.xrCreateSwapchain(session, &swapchainInfo, &mSwapchain);
  if (nextResult != XR_SUCCESS) {
    mSwapchain = nullptr;
    dprintf("Failed to create swapchain: {}", nextResult);
    return;
  }

  uint32_t imageCount = 0;
  nextResult
    = gNext.xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || nextResult != XR_SUCCESS) {
    dprintf("No images in swapchain: {}", nextResult);
    return;
  }

  dprintf("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageD3D11KHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageD3D11KHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,
    });
  nextResult = gNext.xrEnumerateSwapchainImages(
    mSwapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to enumerate images in swapchain: {}", nextResult);
    gNext.xrDestroySwapchain(mSwapchain);
    mSwapchain = nullptr;
    return;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
    dprint("Swap chain is not a D3D11 swapchain");
    OPENKNEEBOARD_BREAK;
    gNext.xrDestroySwapchain(mSwapchain);
    mSwapchain = nullptr;
    return;
  }

  mTextures.resize(imageCount);
  for (size_t i = 0; i < imageCount; ++i) {
#ifdef DEBUG
    if (images.at(i).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
      OPENKNEEBOARD_BREAK;
    }
#endif
    mTextures.at(i).copy_from(images.at(i).texture);
  }

  dprint("Initialized kneeboard");
}

OpenXRD3D11Kneeboard::~OpenXRD3D11Kneeboard() {
  dprintf("{}", __FUNCTION__);
  if (mSwapchain) {
    gNext.xrDestroySwapchain(mSwapchain);
  }
}

static XrPosef xrPoseFromSimpleMathMatrix(const Matrix& m) {
  const auto pos = m.Translation();
  const auto quat = Quaternion::CreateFromRotationMatrix(m);

  return XrPosef {
    .orientation = {.x = quat.x, .y = quat.y, .z = quat.z, .w = quat.w},
    .position = {pos.x, pos.y, pos.z},
  };
}

XrResult OpenXRD3D11Kneeboard::xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo) {
  if (frameEndInfo->layerCount == 0) {
    return gNext.xrEndFrame(session, frameEndInfo);
  }

  auto snapshot = mSHM.MaybeGet();
  if (!mSwapchain) {
    throw std::logic_error("Missing swapchain");
    return gNext.xrEndFrame(session, frameEndInfo);
  }
  if (!snapshot) {
    // Don't spam: expected, if OpenKneeboard isn't running
    return gNext.xrEndFrame(session, frameEndInfo);
  }
  auto config = snapshot.GetConfig();

  if (snapshot.GetSequenceNumber() != mSequenceNumber) {
    this->Render(snapshot);
  }

  const auto& vr = config.vr;
  if (vr.recenterCount != mRecenterCount) {
    this->Recenter(frameEndInfo->displayTime);
    mRecenterCount = vr.recenterCount;
  }

  std::vector<const XrCompositionLayerBaseHeader*> nextLayers;
  std::copy(
    frameEndInfo->layers,
    &frameEndInfo->layers[frameEndInfo->layerCount],
    std::back_inserter(nextLayers));

  const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
  const auto virtualHeight = vr.height;
  const auto virtualWidth = aspectRatio * vr.height;

  const XrExtent2Df normalSize {virtualWidth, virtualHeight};
  const XrExtent2Df zoomedSize {
    virtualWidth * vr.zoomScale, virtualHeight * vr.zoomScale};

  // clang-format off
  auto kneeboardPose =
    Matrix::CreateRotationX(vr.rx)
    * Matrix::CreateRotationY(vr.ry)
    * Matrix::CreateRotationZ(vr.rz)
    * Matrix::CreateTranslation({vr.x, vr.eyeY, vr.z})
    * mRecenter;
  // clang-format on

  static_assert(
    SHM::SHARED_TEXTURE_IS_PREMULTIPLIED,
    "Use premultiplied alpha in shared texture, or pass "
    "XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT");
  XrCompositionLayerQuad layer {
    .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
    .next = nullptr,
    .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT 
      | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
    .space = mLocalSpace,
    .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
    .subImage = {
      .swapchain = mSwapchain,
      .imageRect = {
        {0, 0},
        {static_cast<int32_t>(config.imageWidth), static_cast<int32_t>(config.imageHeight)},
      },
      .imageArrayIndex = 0,
    },
    .pose = xrPoseFromSimpleMathMatrix(kneeboardPose),
    .size = this->GetSize(config, kneeboardPose, frameEndInfo->displayTime),
  };
  nextLayers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));

  XrFrameEndInfo nextFrameEndInfo {*frameEndInfo};
  nextFrameEndInfo.layers = nextLayers.data();
  nextFrameEndInfo.layerCount = static_cast<uint32_t>(nextLayers.size());

  auto nextResult = gNext.xrEndFrame(session, &nextFrameEndInfo);
  if (nextResult != XR_SUCCESS) {
    OPENKNEEBOARD_BREAK;
  }
  return nextResult;
}

void OpenXRD3D11Kneeboard::Render(const SHM::Snapshot& snapshot) {
  auto config = snapshot.GetConfig();
  uint32_t textureIndex;
  auto nextResult
    = gNext.xrAcquireSwapchainImage(mSwapchain, nullptr, &textureIndex);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to acquire swapchain image: {}", nextResult);
    return;
  }

  XrSwapchainImageWaitInfo waitInfo {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
    .timeout = XR_INFINITE_DURATION,
  };
  nextResult = gNext.xrWaitSwapchainImage(mSwapchain, &waitInfo);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to wait for swapchain image: {}", nextResult);
    nextResult = gNext.xrReleaseSwapchainImage(mSwapchain, nullptr);
    if (nextResult != XR_SUCCESS) {
      dprintf("Failed to release swapchain image: {}", nextResult);
    }
    return;
  }

  auto texture = mTextures.at(textureIndex).get();

  {
    auto sharedTexture = snapshot.GetSharedTexture(mDevice);
    if (!sharedTexture) {
      dprint("Failed to get shared texture");
      return;
    }

    winrt::com_ptr<ID3D11DeviceContext> context;
    mDevice->GetImmediateContext(context.put());
    D3D11_BOX sourceBox {
      .left = 0,
      .top = 0,
      .front = 0,
      .right = config.imageWidth,
      .bottom = config.imageHeight,
      .back = 1,
    };
    context->CopySubresourceRegion(
      texture, 0, 0, 0, 0, sharedTexture.GetTexture(), 0, &sourceBox);
    context->Flush();
  }
  nextResult = gNext.xrReleaseSwapchainImage(mSwapchain, nullptr);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to release swapchain: {}", nextResult);
  }
  this->mSequenceNumber = snapshot.GetSequenceNumber();
}

// Don't use a unique_ptr as on process exit, windows doesn't clean these up
// in a useful order, and Microsoft recommend just leaking resources on
// thread/process exit
//
// In this case, it leads to an infinite hang on ^C
static OpenXRKneeboard* gKneeboard = nullptr;

template <class T>
static const T* findInXrNextChain(XrStructureType type, const void* next) {
  while (next) {
    auto base = reinterpret_cast<const XrBaseInStructure*>(next);
    if (base->type == type) {
      return reinterpret_cast<const T*>(base);
    }
    next = base->next;
  }
  return nullptr;
}

XrResult xrCreateSession(
  XrInstance instance,
  const XrSessionCreateInfo* createInfo,
  XrSession* session) {
  static PFN_xrCreateSession sNext = nullptr;

  auto nextResult = gNext.xrCreateSession(instance, createInfo, session);
  if (nextResult != XR_SUCCESS) {
    return nextResult;
  }

  if (gKneeboard) {
    dprint("Already have a kneeboard, refusing to initialize twice");
    return XR_ERROR_INITIALIZATION_FAILED;
  }

  auto d3d11 = findInXrNextChain<XrGraphicsBindingD3D11KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, createInfo->next);
  if (d3d11 && d3d11->device) {
    gKneeboard = new OpenXRD3D11Kneeboard(*session, d3d11->device);
    return XR_SUCCESS;
  }

  dprint("Unsupported graphics API");

  return XR_SUCCESS;
}

XrResult xrDestroySession(XrSession session) {
  delete gKneeboard;
  gKneeboard = nullptr;
  return gNext.xrDestroySession(session);
}

XrResult xrDestroyInstance(XrInstance instance) {
  delete gKneeboard;
  gKneeboard = nullptr;
  return gNext.xrDestroyInstance(instance);
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) {
  if (gKneeboard) {
    return gKneeboard->xrEndFrame(session, frameEndInfo);
  }
  return gNext.xrEndFrame(session, frameEndInfo);
}

XrResult xrGetInstanceProcAddr(
  XrInstance instance,
  const char* name_cstr,
  PFN_xrVoidFunction* function) {
  std::string_view name {name_cstr};

  if (name == "xrCreateSession") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrCreateSession);
    return XR_SUCCESS;
  }
  if (name == "xrDestroySession") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrDestroySession);
    return XR_SUCCESS;
  }
  if (name == "xrDestroyInstance") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrDestroyInstance);
    return XR_SUCCESS;
  }
  if (name == "xrEndFrame") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrEndFrame);
    return XR_SUCCESS;
  }

  return gNext.xrGetInstanceProcAddr(instance, name_cstr, function);
}

XrResult xrCreateApiLayerInstance(
  const XrInstanceCreateInfo* info,
  const struct XrApiLayerCreateInfo* layerInfo,
  XrInstance* instance) {
  dprintf("{}", __FUNCTION__);
  // TODO: check version fields etc in layerInfo
  gNext.xrGetInstanceProcAddr = layerInfo->nextInfo->nextGetInstanceProcAddr;

  XrApiLayerCreateInfo nextLayerInfo = *layerInfo;
  nextLayerInfo.nextInfo = layerInfo->nextInfo->next;
  auto nextResult = layerInfo->nextInfo->nextCreateApiLayerInstance(
    info, &nextLayerInfo, instance);
  if (nextResult != XR_SUCCESS) {
    dprint("Next failed.");
    return nextResult;
  }

#define IT(x) \
  if ( \
    gNext.xrGetInstanceProcAddr( \
      *instance, #x, reinterpret_cast<PFN_xrVoidFunction*>(&gNext.x)) \
    != XR_SUCCESS) { \
    return XR_ERROR_INITIALIZATION_FAILED; \
  }
  NEEDED_OPENXR_FUNCS
#undef IT

  dprint("Created API layer instance");

  return XR_SUCCESS;
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

extern "C" {
XrResult __declspec(dllexport) XRAPI_CALL
  OpenKneeboard_xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo* loaderInfo,
    const char* layerName,
    XrNegotiateApiLayerRequest* apiLayerRequest) {
  DPrintSettings::Set({
    .prefix = "OpenKneeboard-OpenXR",
    .target = DPrintSettings::Target::CONSOLE_AND_DEBUG_STREAM,
    .prefixTarget = DPrintSettings::Target::CONSOLE_AND_DEBUG_STREAM,
  });
  dprintf("{}", __FUNCTION__);

  if (layerName != OpenKneeboard::OpenXRLayerName) {
    dprintf("Layer name mismatch:\n -{}\n +{}", OpenXRLayerName, layerName);
    return XR_ERROR_INITIALIZATION_FAILED;
  }

  // TODO: check version fields etc in loaderInfo

  apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
  apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
  apiLayerRequest->getInstanceProcAddr = &OpenKneeboard::xrGetInstanceProcAddr;
  apiLayerRequest->createApiLayerInstance
    = &OpenKneeboard::xrCreateApiLayerInstance;
  return XR_SUCCESS;
}
}
