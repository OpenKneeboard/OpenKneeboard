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

const std::string_view OpenXRLayerName {"XR_APILAYER_NOVENDOR_OpenKneeboard"};

#define NEEDED_OPENXR_FUNCS \
  IT(xrCreateSession) \
  IT(xrEndFrame) \
  IT(xrCreateSwapchain) \
  IT(xrEnumerateSwapchainImages) \
  IT(xrAcquireSwapchainImage) \
  IT(xrReleaseSwapchainImage) \
  IT(xrDestroySwapchain) \
  IT(xrCreateReferenceSpace) \
  IT(xrDestroySpace)

static struct {
  PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr {nullptr};
#define IT(x) PFN_##x x {nullptr};
  NEEDED_OPENXR_FUNCS
#undef IT
} gOpenXR;

class OpenXRKneeboard {
 public:
  virtual ~OpenXRKneeboard();
  virtual XrResult xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo)
    = 0;

 private:
  XrSwapchain mSwapChain = nullptr;
};

class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(XrSession, ID3D11Device*);
  ~OpenXRD3D11Kneeboard();

  virtual XrResult xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo) override;

 private:
  SHM::Reader mSHM;
  ID3D11Device* mDevice = nullptr;
  XrSwapchain mSwapchain = nullptr;
  XrSpace mSpace = nullptr;
  std::vector<winrt::com_ptr<ID3D11Texture2D>> mTextures;
};

OpenXRKneeboard::~OpenXRKneeboard() {
}

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrSession session,
  ID3D11Device* device)
  : mDevice(device) {
  dprintf("{}", __FUNCTION__);
  XrReferenceSpaceCreateInfo referenceSpace {
    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
    .next = nullptr,
    .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
    .poseInReferenceSpace = {
      {0.0f, 0.0f, 0.0f, 1.0f},
      {0.0f, 0.0f, 0.0f},
    },
  };

  XrResult nextResult
    = gOpenXR.xrCreateReferenceSpace(session, &referenceSpace, &mSpace);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to create reference space: {}", nextResult);
    return;
  }
  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
    .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    .sampleCount = 1,
    .width = TextureWidth,
    .height = TextureHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };
  nextResult = gOpenXR.xrCreateSwapchain(session, &swapchainInfo, &mSwapchain);
  if (nextResult != XR_SUCCESS) {
    mSwapchain = nullptr;
    dprintf("Failed to create swapchain: {}", nextResult);
    return;
  }

  uint32_t imageCount = 0;
  nextResult
    = gOpenXR.xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr);
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
  nextResult = gOpenXR.xrEnumerateSwapchainImages(
    mSwapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to enumerate images in swapchain: {}", nextResult);
    gOpenXR.xrDestroySwapchain(mSwapchain);
    mSwapchain = nullptr;
    return;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
    dprint("Swap chain is not a D3D11 swapchain");
    OPENKNEEBOARD_BREAK;
    gOpenXR.xrDestroySwapchain(mSwapchain);
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
  if (mSpace) {
    gOpenXR.xrDestroySpace(mSpace);
  }
  if (mSwapchain) {
    gOpenXR.xrDestroySwapchain(mSwapchain);
  }
}

XrResult OpenXRD3D11Kneeboard::xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo) {
  auto snapshot = mSHM.MaybeGet();
  if (!mSwapchain) {
    dprint("Missing swapchain");
    return gOpenXR.xrEndFrame(session, frameEndInfo);
  }
  if (!snapshot) {
    dprint("Missing snapshot");
    return gOpenXR.xrEndFrame(session, frameEndInfo);
  }

  auto sharedTexture = snapshot.GetSharedTexture(mDevice);
  if (!sharedTexture) {
    dprint("Failed to get shared texture");
    return gOpenXR.xrEndFrame(session, frameEndInfo);
  }

  uint32_t textureIndex;
  if (
    gOpenXR.xrAcquireSwapchainImage(mSwapchain, nullptr, &textureIndex)
    != XR_SUCCESS) {
    dprint("Failed to acquire swapchain image");
    return gOpenXR.xrEndFrame(session, frameEndInfo);
  }

  auto texture = mTextures.at(textureIndex).get();

  winrt::com_ptr<ID3D11DeviceContext> context;
  mDevice->GetImmediateContext(context.put());

  auto config = snapshot.GetConfig();
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
  gOpenXR.xrReleaseSwapchainImage(mSwapchain, nullptr);

  std::vector<const XrCompositionLayerBaseHeader*> nextLayers;
  std::copy(
    frameEndInfo->layers,
    &frameEndInfo->layers[frameEndInfo->layerCount],
    std::back_inserter(nextLayers));

  const auto& vr = config.vr;
  const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
  const auto virtualHeight = vr.height;
  const auto virtualWidth = aspectRatio * vr.height;

  // Using a variable to easily substitute zoom later
  const XrExtent2Df size {virtualWidth, virtualHeight};

  auto quat = Quaternion::CreateFromYawPitchRoll({
    vr.rx,
    vr.ry,
    vr.rz,
  });

  static_assert(
    SHM::SHARED_TEXTURE_IS_PREMULTIPLIED,
    "Use premultiplied alpha in shared texture, or pass "
    "XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT");
  XrCompositionLayerQuad layer {
    .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
    .next = nullptr,
    .layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT
      | XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
    .space = mSpace,
    .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
    .subImage = {
      .swapchain = mSwapchain,
      .imageRect = {
        {0, 0},
        {static_cast<int32_t>(config.imageWidth), static_cast<int32_t>(config.imageHeight)},
      },
      .imageArrayIndex = textureIndex,
    },
    .pose = {
      .orientation = {quat.x, quat.y, quat.z, quat.w },
      .position = { vr.x, vr.floorY, vr.z },
    },
    .size = size,
  };
  nextLayers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));

  XrFrameEndInfo nextFrameEndInfo {*frameEndInfo};
  nextFrameEndInfo.layers = nextLayers.data();
  nextFrameEndInfo.layerCount = static_cast<uint32_t>(nextLayers.size());

  dprint("Passing to next");

  return gOpenXR.xrEndFrame(session, &nextFrameEndInfo);
}

static std::unique_ptr<OpenXRKneeboard> gKneeboard;

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

  auto nextResult = gOpenXR.xrCreateSession(instance, createInfo, session);
  if (nextResult != XR_SUCCESS) {
    return nextResult;
  }

  auto d3d11 = findInXrNextChain<XrGraphicsBindingD3D11KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, createInfo->next);
  if (d3d11 && d3d11->device) {
    gKneeboard
      = std::make_unique<OpenXRD3D11Kneeboard>(*session, d3d11->device);
    return XR_SUCCESS;
  }

  dprint("Unsupported graphics API");

  return XR_SUCCESS;
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) {
  if (gKneeboard) {
    return gKneeboard->xrEndFrame(session, frameEndInfo);
  }
  return gOpenXR.xrEndFrame(session, frameEndInfo);
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
  if (name == "xrEndFrame") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrEndFrame);
    return XR_SUCCESS;
  }

  return gOpenXR.xrGetInstanceProcAddr(instance, name_cstr, function);
}

XrResult xrCreateApiLayerInstance(
  const XrInstanceCreateInfo* info,
  const struct XrApiLayerCreateInfo* layerInfo,
  XrInstance* instance) {
  dprintf("{}", __FUNCTION__);
  // TODO: check version fields etc in layerInfo
  gOpenXR.xrGetInstanceProcAddr = layerInfo->nextInfo->nextGetInstanceProcAddr;

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
    gOpenXR.xrGetInstanceProcAddr( \
      *instance, #x, reinterpret_cast<PFN_xrVoidFunction*>(&gOpenXR.x)) \
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
