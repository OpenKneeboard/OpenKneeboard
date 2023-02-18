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

#include <OpenKneeboard/d3d11.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <dxgi1_6.h>
#include <vulkan/vulkan.h>

#include "OpenXRNext.h"

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

template <class CharT>
struct std::formatter<VkResult, CharT> : std::formatter<int, CharT> {};

static inline constexpr bool VK_SUCCEEDED(VkResult code) {
  return code >= 0;
}

static inline constexpr bool VK_FAILED(VkResult code) {
  return !VK_SUCCEEDED(code);
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
    mVKDevice(binding.device),
    mVKAllocator(vulkanAllocator) {
  dprintf("{}", __FUNCTION__);

  this->InitializeVulkan(binding, pfnVkGetInstanceProcAddr);

  if (!mVKCommandPool) {
    return;
  }

  this->InitializeD3D11(binding);
}

void OpenXRVulkanKneeboard::InitializeVulkan(
  const XrGraphicsBindingVulkanKHR& binding,
  PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr) {
#define IT(vkfun) \
  mPFN_##vkfun = reinterpret_cast<PFN_##vkfun>( \
    pfnVkGetInstanceProcAddr(binding.instance, #vkfun));
  OPENKNEEBOARD_VK_FUNCS
#undef IT

  mPFN_vkGetDeviceQueue(
    binding.device, binding.queueFamilyIndex, binding.queueIndex, &mVKQueue);

  {
    VkCommandPoolCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = binding.queueFamilyIndex,
    };

    const auto ret = mPFN_vkCreateCommandPool(
      binding.device, &createInfo, mVKAllocator, &mVKCommandPool);
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
      binding.device, &allocateInfo, &mVKCommandBuffer);
    if (XR_FAILED(ret)) {
      dprintf("Failed to create command buffer: {}", ret);
      return;
    }
  }

  {
    VkFenceCreateInfo createInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    mPFN_vkCreateFence(
      binding.device, &createInfo, mVKAllocator, &mInteropVKFence);
  }
}

void OpenXRVulkanKneeboard::InitializeD3D11(
  const XrGraphicsBindingVulkanKHR& binding) {
  VkPhysicalDeviceIDProperties deviceIDProps {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};

  VkPhysicalDeviceProperties2 deviceProps2 {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  deviceProps2.pNext = &deviceIDProps;
  mPFN_vkGetPhysicalDeviceProperties2(binding.physicalDevice, &deviceProps2);

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

  winrt::check_hresult(D3D11CreateDevice(
    adapter.get(),
    // UNKNOWN is reuqired when specifying an adapter
    D3D_DRIVER_TYPE_UNKNOWN,
    nullptr,
    d3dFlags,
    &d3dLevel,
    1,
    D3D11_SDK_VERSION,
    mD3D11Device.put(),
    nullptr,
    nullptr));
  dprint("Initialized D3D11 device matching VkPhysicalDevice");

  mD3D11Texture = SHM::CreateCompatibleTexture(
    mD3D11Device.get(),
    D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    D3D11_RESOURCE_MISC_SHARED);
  winrt::check_hresult(mD3D11Device->CreateRenderTargetView(
    mD3D11Texture.get(), nullptr, mD3D11RenderTargetView.put()));

  HANDLE sharedHandle {};
  winrt::check_hresult(
    mD3D11Texture.as<IDXGIResource>()->GetSharedHandle(&sharedHandle));

  {
    // Specifying VK_FORMAT_B8G8R8A8_UNORM below
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
      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .extent = {TextureWidth, TextureHeight, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage
      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &binding.queueFamilyIndex,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const auto ret = mPFN_vkCreateImage(
      binding.device, &createInfo, mVKAllocator, &mInteropVKImage);
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
      .image = mInteropVKImage,
    };

    VkMemoryRequirements2 memoryRequirements {
      VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };

    mPFN_vkGetImageMemoryRequirements2(
      binding.device, &info, &memoryRequirements);

    VkMemoryWin32HandlePropertiesKHR handleProperties {
      .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
    };
    mPFN_vkGetMemoryWin32HandlePropertiesKHR(
      binding.device,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT,
      sharedHandle,
      &handleProperties);

    ///// Which memory areas meet those requirements? /////

    VkPhysicalDeviceMemoryProperties memoryProperties {};
    mPFN_vkGetPhysicalDeviceMemoryProperties(
      binding.physicalDevice, &memoryProperties);

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
      .image = mInteropVKImage,
    };

    VkMemoryAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &dedicatedAllocInfo,
      .allocationSize = memoryRequirements.memoryRequirements.size,
      .memoryTypeIndex = *memoryTypeIndex,
    };
    const auto ret = mPFN_vkAllocateMemory(
      binding.device, &allocInfo, mVKAllocator, &memory);
    if (VK_FAILED(ret)) {
      dprintf("Failed to allocate memory for interop: {}", ret);
      return;
    }
  }

  {
    VkBindImageMemoryInfo bindImageInfo {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .image = mInteropVKImage,
      .memory = memory,
    };

    const auto res = mPFN_vkBindImageMemory2(binding.device, 1, &bindImageInfo);
    if (XR_FAILED(res)) {
      dprintf("Failed to bind image memory: {}", res);
    }
  }
}

OpenXRVulkanKneeboard::~OpenXRVulkanKneeboard() = default;

bool OpenXRVulkanKneeboard::ConfigurationsAreCompatible(
  const VRRenderConfig& /*initial*/,
  const VRRenderConfig& /*current*/) const {
  return true;
}

winrt::com_ptr<ID3D11Device> OpenXRVulkanKneeboard::GetD3D11Device() const {
  return mD3D11Device;
}

XrSwapchain OpenXRVulkanKneeboard::CreateSwapChain(
  XrSession session,
  const VRRenderConfig& vrc,
  uint8_t layerIndex) {
  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  const auto vkFormat = VK_FORMAT_B8G8R8A8_UNORM;
  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
    .format = vkFormat,
    .sampleCount = 1,
    .width = TextureWidth,
    .height = TextureHeight,
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

  for (const auto& swapchainImage: images) {
    mImages[layerIndex].push_back(swapchainImage.image);
  }

  return swapchain;
}

bool OpenXRVulkanKneeboard::Render(
  XrSwapchain swapchain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters& params) {
  if (!swapchain) {
    dprint("asked to render without swapchain");
    return false;
  }

  if (!mD3D11Device) {
    dprint("asked to render without D3D11 device");
    return false;
  }

  const auto oxr = this->GetOpenXR();
  const auto config = snapshot.GetConfig();
  const auto layerConfig = snapshot.GetLayerConfig(layerIndex);

  uint32_t textureIndex;
  {
    const auto res
      = oxr->xrAcquireSwapchainImage(swapchain, nullptr, &textureIndex);
    if (XR_FAILED(res)) {
      dprintf("Failed to acquire swapchain image: {}", res);
      return false;
    }
  }
  const scope_guard releaseSwapchainImage([swapchain, oxr]() {
    const auto res = oxr->xrReleaseSwapchainImage(swapchain, nullptr);
    if (XR_FAILED(res)) {
      dprintf("Failed to release swapchain image: {}", res);
    }
  });

  XrSwapchainImageWaitInfo waitInfo {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
    .timeout = XR_INFINITE_DURATION,
  };
  {
    const auto res = oxr->xrWaitSwapchainImage(swapchain, &waitInfo);
    if (XR_FAILED(res)) {
      dprintf("Failed to wait for swapchain image: {}", res);
      return false;
    }
  }

  const auto srv
    = snapshot.GetLayerShaderResourceView(mD3D11Device.get(), layerIndex);
  if (!srv) {
    dprint("Failed to get layer SRV");
    return false;
  }

  D3D11::CopyTextureWithOpacity(
    mD3D11Device.get(),
    srv.get(),
    mD3D11RenderTargetView.get(),
    params.mKneeboardOpacity);

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
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = mInteropVKImage,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    },
  };
  const auto dstImage = mImages.at(layerIndex).at(textureIndex);
  inBarriers[1] = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
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
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    /* dependency flags = */ {},
    /* memory barrier count = */ 0,
    /* memory barriers = */ nullptr,
    /* buffer barrier count = */ 0,
    /* buffer barriers = */ nullptr,
    /* image barrier count = */ std::size(inBarriers),
    inBarriers);

  VkImageCopy imageCopy {
    .srcSubresource = VkImageSubresourceLayers {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .srcOffset = {0, 0, 0},
    .dstSubresource = VkImageSubresourceLayers {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .dstOffset = {0, 0, 0},
    .extent = { layerConfig->mImageWidth, layerConfig->mImageHeight, 1 },
  };
  mPFN_vkCmdCopyImage(
    mVKCommandBuffer,
    mInteropVKImage,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    mImages.at(layerIndex).at(textureIndex),
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &imageCopy);

  VkImageMemoryBarrier outBarrier {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
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
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    /* dependency flags = */ {},
    /* memory barrier count = */ 0,
    /* memory barriers = */ nullptr,
    /* buffer barrier count = */ 0,
    /* buffer barriers = */ nullptr,
    /* image barrier count = */ 1,
    &outBarrier);
  mPFN_vkEndCommandBuffer(mVKCommandBuffer);

  {
    VkSubmitInfo submitInfo {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &mVKCommandBuffer,
    };

    mPFN_vkResetFences(mVKDevice, 1, &mInteropVKFence);
    const auto res
      = mPFN_vkQueueSubmit(mVKQueue, 1, &submitInfo, mInteropVKFence);
    if (VK_FAILED(res)) {
      dprintf("Queue submit failed: {}", res);
      return false;
    }
  }

  mPFN_vkWaitForFences(
    mVKDevice,
    1,
    &mInteropVKFence,
    VK_TRUE,
    std::numeric_limits<uint64_t>::max());

  return true;
}

}// namespace OpenKneeboard
