/*
 * OpenKneeboard
 *
 * Copyright (C) 2022-present Fred Emmott <fred@fredemmott.com>
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

#include "OpenXRVulkanKneeboard.h"

#include "OpenXRD3D11Kneeboard.h"
#include "OpenXRNext.h"

#include <OpenKneeboard/d3d11.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

#include <vulkan/vulkan.h>

#include <d3d11_4.h>
#include <dxgi1_6.h>

template <class CharT>
struct std::formatter<VkResult, CharT> : std::formatter<int, CharT> {};

static inline constexpr bool VK_SUCCEEDED(VkResult code) {
  return code >= 0;
}

static inline constexpr bool VK_FAILED(VkResult code) {
  return !VK_SUCCEEDED(code);
}

static inline void check_vkresult(VkResult code) {
  if (VK_FAILED(code)) {
    throw std::runtime_error(
      std::format("Vulkan call failed: {}", static_cast<int>(code)));
  }
}

namespace OpenKneeboard {

OpenXRVulkanKneeboard::OpenXRVulkanKneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingVulkanKHR& binding,
  const VkAllocationCallbacks* vulkanAllocator,
  PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr)
  : OpenXRKneeboard(session, runtimeID, next),
    mBinding(binding),
    mVKDevice(binding.device),
    mVKAllocator(vulkanAllocator) {
  dprintf("{}", __FUNCTION__);
  TraceLoggingWrite(gTraceProvider, "OpenXRVulkanKneeboard()");

  this->InitializeVulkan(pfnVkGetInstanceProcAddr);

  if (!mVKCommandPool) {
    return;
  }

  this->InitializeD3D11();
}

void OpenXRVulkanKneeboard::InitializeVulkan(
  PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr) {
#define IT(vkfun) \
  mPFN_##vkfun = reinterpret_cast<PFN_##vkfun>( \
    pfnVkGetInstanceProcAddr(mBinding.instance, #vkfun));
  OPENKNEEBOARD_VK_FUNCS
#undef IT

  mPFN_vkGetDeviceQueue(
    mVKDevice, mBinding.queueFamilyIndex, mBinding.queueIndex, &mVKQueue);

  {
    VkCommandPoolCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = mBinding.queueFamilyIndex,
    };

    const auto ret = mPFN_vkCreateCommandPool(
      mVKDevice, &createInfo, mVKAllocator, &mVKCommandPool);
    if (VK_FAILED(ret)) {
      dprintf("Failed to create command pool: {}", ret);
      return;
    }
  }

  {
    VkCommandBufferAllocateInfo allocateInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = mVKCommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };

    const auto ret = mPFN_vkAllocateCommandBuffers(
      mVKDevice, &allocateInfo, &mVKCommandBuffer);
    if (VK_FAILED(ret)) {
      dprintf("Failed to create command buffer: {}", ret);
      return;
    }
  }
}

void OpenXRVulkanKneeboard::InitializeD3D11() {
  VkPhysicalDeviceIDProperties deviceIDProps {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};

  VkPhysicalDeviceProperties2 deviceProps2 {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  deviceProps2.pNext = &deviceIDProps;
  mPFN_vkGetPhysicalDeviceProperties2(mBinding.physicalDevice, &deviceProps2);

  if (deviceIDProps.deviceLUIDValid == VK_FALSE) {
    dprint("Failed to get Vulkan device LUID");
    return;
  }

  UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  auto d3dLevel = D3D_FEATURE_LEVEL_11_1;
  UINT dxgiFlags = 0;

#ifdef DEBUG
  d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
  dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  winrt::com_ptr<IDXGIFactory> dxgiFactory;
  winrt::check_hresult(
    CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(dxgiFactory.put())));

  winrt::com_ptr<IDXGIAdapter> adapterIt;
  winrt::com_ptr<IDXGIAdapter> adapter;
  for (unsigned int i = 0;
       dxgiFactory->EnumAdapters(i, adapterIt.put()) == S_OK;
       ++i) {
    const scope_guard releaseAdapter([&]() { adapterIt = {nullptr}; });

    DXGI_ADAPTER_DESC desc;
    winrt::check_hresult(adapterIt->GetDesc(&desc));

    dprintf(L"Checking D3D11 device '{}'", desc.Description);

    if (
      memcmp(&desc.AdapterLuid, deviceIDProps.deviceLUID, sizeof(LUID)) != 0) {
      dprint("VK and D3D11 LUIDs don't match.");
      OPENKNEEBOARD_BREAK;
      continue;
    }

    dprintf(L"VK and D3D11 LUIDs match.", desc.Description);
    adapter = adapterIt;
    break;
  }
  if (!adapter) {
    dprint("Couldn't find DXGI adapter with LUID matching VkPhysicalDevice");
    return;
  }

  winrt::com_ptr<ID3D11Device> d3d11Device;
  winrt::com_ptr<ID3D11DeviceContext> d3d11ImmediateContext;
  winrt::check_hresult(D3D11CreateDevice(
    adapter.get(),
    // UNKNOWN is reuqired when specifying an adapter
    D3D_DRIVER_TYPE_UNKNOWN,
    nullptr,
    d3dFlags,
    &d3dLevel,
    1,
    D3D11_SDK_VERSION,
    d3d11Device.put(),
    nullptr,
    d3d11ImmediateContext.put()));
  dprint("Initialized D3D11 device matching VkPhysicalDevice");
  mD3D11Device = d3d11Device.as<ID3D11Device5>();
  mD3D11ImmediateContext = d3d11ImmediateContext.as<ID3D11DeviceContext4>();
}

void OpenXRVulkanKneeboard::InitInterop(
  const PixelSize& textureSize,
  Interop* interop) noexcept {
  auto device = mD3D11Device.get();
  interop->mD3D11Texture = SHM::CreateCompatibleTexture(
    device,
    D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    D3D11_RESOURCE_MISC_SHARED);
  winrt::check_hresult(device->CreateRenderTargetView(
    interop->mD3D11Texture.get(),
    nullptr,
    interop->mD3D11RenderTargetView.put()));

  HANDLE sharedHandle {};
  winrt::check_hresult(
    interop->mD3D11Texture.as<IDXGIResource>()->GetSharedHandle(&sharedHandle));

  {
    // Specifying VK_FORMAT_ below
    static_assert(
      SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);

    VkExternalMemoryImageCreateInfo externalCreateInfo {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT,
    };

    VkImageCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = &externalCreateInfo,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_B8G8R8A8_SRGB,
      .extent = {textureSize.mWidth, textureSize.mHeight, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage
      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &mBinding.queueFamilyIndex,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const auto ret = mPFN_vkCreateImage(
      mVKDevice, &createInfo, mVKAllocator, &interop->mVKImage);
    if (VK_FAILED(ret)) {
      dprintf("Failed to create interop image: {}", ret);
      return;
    }
  }

  VkDeviceMemory memory {};
  {
    ///// What kind of memory do we need? /////

    VkImageMemoryRequirementsInfo2 info {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .image = interop->mVKImage,
    };

    VkMemoryRequirements2 memoryRequirements {
      VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };

    mPFN_vkGetImageMemoryRequirements2(mVKDevice, &info, &memoryRequirements);

    VkMemoryWin32HandlePropertiesKHR handleProperties {
      .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
    };
    mPFN_vkGetMemoryWin32HandlePropertiesKHR(
      mVKDevice,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT,
      sharedHandle,
      &handleProperties);

    ///// Which memory areas meet those requirements? /////

    VkPhysicalDeviceMemoryProperties memoryProperties {};
    mPFN_vkGetPhysicalDeviceMemoryProperties(
      mBinding.physicalDevice, &memoryProperties);

    std::optional<uint32_t> memoryTypeIndex;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
      const auto typeBit = 1ui32 << i;
      if (!(handleProperties.memoryTypeBits & typeBit)) {
        continue;
      }

      if (!(memoryProperties.memoryTypes[i].propertyFlags
            & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        continue;
      }
      memoryTypeIndex = i;
      break;
    }

    if (!memoryTypeIndex) {
      dprint("Failed to find a memory region suitable for VK/DX interop");
      return;
    }

    ///// Finally, allocate the memory :) /////

    VkImportMemoryWin32HandleInfoKHR handleInfo {
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT,
      .handle = sharedHandle,
    };

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .pNext = &handleInfo,
      .image = interop->mVKImage,
    };

    VkMemoryAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &dedicatedAllocInfo,
      .allocationSize = memoryRequirements.memoryRequirements.size,
      .memoryTypeIndex = *memoryTypeIndex,
    };
    const auto ret
      = mPFN_vkAllocateMemory(mVKDevice, &allocInfo, mVKAllocator, &memory);
    if (VK_FAILED(ret)) {
      dprintf("Failed to allocate memory for interop: {}", ret);
      return;
    }
  }

  {
    VkBindImageMemoryInfo bindImageInfo {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .image = interop->mVKImage,
      .memory = memory,
    };

    const auto res = mPFN_vkBindImageMemory2(mVKDevice, 1, &bindImageInfo);
    if (VK_FAILED(res)) {
      dprintf("Failed to bind image memory: {}", res);
    }
  }

  {
    VkFenceCreateInfo createInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    check_vkresult(mPFN_vkCreateFence(
      mVKDevice, &createInfo, mVKAllocator, &interop->mVKCompletionFence));
  }

  // 1/3: Create a D3D11 fence...
  winrt::check_hresult(mD3D11Device->CreateFence(
    0,
    D3D11_FENCE_FLAG_SHARED,
    IID_PPV_ARGS(interop->mD3D11InteropFence.put())));

  // 2/3: Create a Vulkan semaphore...
  {
    VkSemaphoreTypeCreateInfoKHR timelineCreateInfo {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
    };
    VkSemaphoreCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &timelineCreateInfo,
    };
    check_vkresult(mPFN_vkCreateSemaphore(
      mVKDevice, &createInfo, mVKAllocator, &interop->mVKInteropSemaphore));
  }

  // 3/3: tie them together
  winrt::check_hresult(interop->mD3D11InteropFence->CreateSharedHandle(
    nullptr, GENERIC_ALL, nullptr, interop->mD3D11InteropFenceHandle.put()));
  {
    VkImportSemaphoreWin32HandleInfoKHR handleInfo {
      .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .semaphore = interop->mVKInteropSemaphore,
      .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT,
      .handle = interop->mD3D11InteropFenceHandle.get(),
    };
    check_vkresult(
      mPFN_vkImportSemaphoreWin32HandleKHR(mVKDevice, &handleInfo));
  }
}

OpenXRVulkanKneeboard::~OpenXRVulkanKneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRVulkanKneeboard()");
}

bool OpenXRVulkanKneeboard::ConfigurationsAreCompatible(
  const VRRenderConfig& /*initial*/,
  const VRRenderConfig& /*current*/) const {
  return true;
}

winrt::com_ptr<ID3D11Device> OpenXRVulkanKneeboard::GetD3D11Device() {
  return mD3D11Device;
}

XrSwapchain OpenXRVulkanKneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size,
  const VRRenderConfig::Quirks&) {
  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  const auto vkFormat = VK_FORMAT_B8G8R8A8_SRGB;
  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
    .format = vkFormat,
    .sampleCount = 1,
    .width = size.mWidth,
    .height = size.mHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };

  auto oxr = this->GetOpenXR();

  XrSwapchain swapchain {nullptr};
  {
    const auto ret
      = oxr->xrCreateSwapchain(session, &swapchainInfo, &swapchain);
    if (XR_FAILED(ret)) {
      dprintf("next->xrCreateSwapchain failed: {}", ret);
      return nullptr;
    }
  }

  uint32_t imageCount = 0;
  {
    const auto res
      = oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
    if (XR_FAILED(res) || imageCount == 0) {
      dprintf("No images in swapchain: {}", res);
      return nullptr;
    }
  }

  dprintf("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageVulkanKHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageVulkanKHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR,
    });
  {
    const auto res = oxr->xrEnumerateSwapchainImages(
      swapchain,
      imageCount,
      &imageCount,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
    if (XR_FAILED(res)) {
      dprintf("Failed to enumerate images in swapchain: {}", res);
      oxr->xrDestroySwapchain(swapchain);
      return nullptr;
    }
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR) {
    dprint("Swap chain is not a Vulkan swapchain");
    OPENKNEEBOARD_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  auto& resources = mSwapchainResources[swapchain];

  for (const auto& swapchainImage: images) {
    resources.mImages.push_back(swapchainImage.image);
  }

  InitInterop(size, &resources.mInterop);
  resources.mSize = size;

  return swapchain;
}

void OpenXRVulkanKneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!mSwapchainResources.contains(swapchain)) {
    return;
  }

  mSwapchainResources.erase(swapchain);
}

bool OpenXRVulkanKneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  uint8_t layerCount,
  LayerRenderInfo* layers) {
  if (!swapchain) {
    dprint("asked to render without swapchain");
    return false;
  }

  if (!mD3D11Device) {
    dprint("asked to render without D3D11 device");
    return false;
  }

  const auto oxr = this->GetOpenXR();

  auto& swapchainResources = mSwapchainResources.at(swapchain);
  auto& interop = swapchainResources.mInterop;

  OpenXRD3D11Kneeboard::RenderLayers(
    oxr,
    mD3D11Device.get(),
    interop.mD3D11RenderTargetView.get(),
    snapshot,
    layerCount,
    layers);

  // Signal this once D3D11 work is done, then make VK wait for it
  const auto semaphoreValue = ++interop.mInteropValue;
  mD3D11ImmediateContext->Signal(
    interop.mD3D11InteropFence.get(), semaphoreValue);

  {
    VkCommandBufferBeginInfo beginInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    const auto ret = mPFN_vkBeginCommandBuffer(mVKCommandBuffer, &beginInfo);
    if (VK_FAILED(ret)) {
      dprintf("Failed to begin command buffer: {}", ret);
      return false;
    }
  }

  VkImageMemoryBarrier inBarriers[2];

  inBarriers[0] = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = interop.mVKImage,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    },
  };
  const auto dstImage = swapchainResources.mImages.at(swapchainTextureIndex);
  inBarriers[1] = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = dstImage,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    },
  };

  mPFN_vkCmdPipelineBarrier(
    mVKCommandBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    /* dependency flags = */ {},
    /* memory barrier count = */ 0,
    /* memory barriers = */ nullptr,
    /* buffer barrier count = */ 0,
    /* buffer barriers = */ nullptr,
    /* image barrier count = */ std::size(inBarriers),
    inBarriers);

  std::vector<VkImageBlit> regions;
  regions.resize(layerCount);
  for (off_t i = 0; i < layerCount; ++i) {
    auto& layer = layers[i];
    const RECT src = layer.mSourceRect;
    const RECT dst = layer.mDestRect;

    regions[i] = VkImageBlit{
    .srcSubresource = VkImageSubresourceLayers {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
      .srcOffsets = {
        {src.left, src.top, 0},
        {src.right, src.bottom, 1},
      },
    .dstSubresource = VkImageSubresourceLayers {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
      .dstOffsets = {
        {dst.left, dst.top, 0},
        {dst.right, dst.bottom, 1},
      }
    };
  }
  mPFN_vkCmdBlitImage(
    mVKCommandBuffer,
    interop.mVKImage,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    dstImage,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    regions.size(),
    regions.data(),
    VK_FILTER_NEAREST);

  VkImageMemoryBarrier outBarriers[2];
  outBarriers[0] = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = interop.mVKImage,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    },
  };
  outBarriers[1] = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = dstImage,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    },
  };

  // Wait for copy to be complete, then...
  mPFN_vkCmdPipelineBarrier(
    mVKCommandBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    /* dependency flags = */ {},
    /* memory barrier count = */ 0,
    /* memory barriers = */ nullptr,
    /* buffer barrier count = */ 0,
    /* buffer barriers = */ nullptr,
    /* image barrier count = */ std::size(outBarriers),
    outBarriers);
  {
    const auto res = mPFN_vkEndCommandBuffer(mVKCommandBuffer);
    if (VK_FAILED(res)) {
      dprintf("vkEndCommandBuffer failed: {}", res);
      return false;
    }
  }

  {
    VkTimelineSemaphoreSubmitInfoKHR timelineSubmitInfo {
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &semaphoreValue,
    };
  VkPipelineStageFlags semaphoreStages {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};
    VkSubmitInfo submitInfo {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &timelineSubmitInfo,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &interop.mVKInteropSemaphore,
      .pWaitDstStageMask = &semaphoreStages,
      .commandBufferCount = 1,
      .pCommandBuffers = &mVKCommandBuffer,
    };

    mPFN_vkResetFences(mVKDevice, 1, &interop.mVKCompletionFence);
    const auto res = mPFN_vkQueueSubmit(
      mVKQueue, 1, &submitInfo, interop.mVKCompletionFence);
    if (VK_FAILED(res)) {
      dprintf("Queue submit failed: {}", res);
      return false;
    }
  }

  {
    const auto res = mPFN_vkWaitForFences(
      mVKDevice,
      1,
      &interop.mVKCompletionFence,
      VK_TRUE,
      std::numeric_limits<uint32_t>::max());
    if (VK_FAILED(res)) {
      dprintf("Waiting for fence failed: {}", res);
      return false;
    }
  }

  return true;
}

}// namespace OpenKneeboard
