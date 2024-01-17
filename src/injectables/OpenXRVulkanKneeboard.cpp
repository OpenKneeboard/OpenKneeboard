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

#include "OpenXRNext.h"

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

#include <vulkan/vulkan.h>

#include <d3d11_4.h>
#include <dxgi1_6.h>

template <class CharT>
struct std::formatter<VkResult, CharT> : std::formatter<int, CharT> {};

using OpenKneeboard::Vulkan::check_vkresult;
using OpenKneeboard::Vulkan::VK_FAILED;

namespace OpenKneeboard {

OpenXRVulkanKneeboard::OpenXRVulkanKneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingVulkanKHR& binding,
  const VkAllocationCallbacks* vulkanAllocator,
  PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr)
  : OpenXRKneeboard(session, runtimeID, next) {
  dprintf("{}", __FUNCTION__);
  TraceLoggingWrite(gTraceProvider, "OpenXRVulkanKneeboard()");

  mVK = std::make_unique<Vulkan::Dispatch>(
    binding.instance, pfnVkGetInstanceProcAddr);
  mDeviceResources = std::make_unique<DeviceResources>(
    mVK.get(),
    binding.device,
    binding.physicalDevice,
    vulkanAllocator,
    binding.queueFamilyIndex,
    binding.queueIndex);
}

OpenXRVulkanKneeboard::DeviceResources::DeviceResources(
  Vulkan::Dispatch* vk,
  VkDevice vkDevice,
  VkPhysicalDevice vkPhysicalDevice,
  const VkAllocationCallbacks* vkAllocator,
  uint32_t queueFamilyIndex,
  uint32_t queueIndex)
  : mVKDevice(vkDevice),
    mVKPhysicalDevice(vkPhysicalDevice),
    mVKAllocator(vkAllocator),
    mVKQueueFamilyIndex(queueFamilyIndex),
    mVKQueueIndex(queueIndex) {
  vk->GetDeviceQueue(vkDevice, queueFamilyIndex, queueIndex, &mVKQueue);

  VkCommandPoolCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = queueFamilyIndex,
  };

  mVKCommandPool
    = vk->make_unique<VkCommandPool>(mVKDevice, &createInfo, mVKAllocator);

  this->InitializeD3D11(vk);
}

void OpenXRVulkanKneeboard::DeviceResources::InitializeD3D11(
  Vulkan::Dispatch* vk) {
  VkPhysicalDeviceIDProperties deviceIDProps {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};

  VkPhysicalDeviceProperties2 deviceProps2 {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  deviceProps2.pNext = &deviceIDProps;
  vk->GetPhysicalDeviceProperties2(mVKPhysicalDevice, &deviceProps2);

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
  mRendererDeviceResources
    = std::make_unique<RendererDeviceResources>(d3d11Device.get());
}

OpenXRVulkanKneeboard::SwapchainResources::InteropResources::InteropResources(
  Vulkan::Dispatch* vk,
  VkDevice vkDevice,
  VkPhysicalDevice vkPhysicalDevice,
  const VkAllocationCallbacks* vkAllocator,
  uint32_t vkQueueFamilyIndex,
  SHM::D3D11::Renderer::DeviceResources* d3d11DeviceResources,
  uint32_t textureCount,
  const PixelSize& textureSize) noexcept {
  auto d3d11Device = d3d11DeviceResources->mD3D11Device.as<ID3D11Device5>();

  std::vector<winrt::com_ptr<ID3D11Texture2D>> d3d11Textures;
  d3d11Textures.reserve(textureCount);
  mVKImages.reserve(textureCount);

  for (uint32_t i = 0; i < textureCount; ++i) {
    winrt::com_ptr<ID3D11Texture2D> d3d11Texture;
    // Not using SHM::CreateCompatibleTexture() as we need to use
    // the specific requested size, not the default SHM size
    D3D11_TEXTURE2D_DESC desc {
      .Width = textureSize.mWidth,
      .Height = textureSize.mHeight,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
    };
    winrt::check_hresult(
      d3d11Device->CreateTexture2D(&desc, nullptr, d3d11Texture.put()));
    d3d11Textures.push_back(d3d11Texture);

    HANDLE sharedHandle {};
    winrt::check_hresult(
      d3d11Texture.as<IDXGIResource>()->GetSharedHandle(&sharedHandle));
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
      .pQueueFamilyIndices = &vkQueueFamilyIndex,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    mVKImages.push_back(
      vk->make_unique<VkImage>(vkDevice, &createInfo, vkAllocator));
    auto vkImage = mVKImages.back().get();

    VkDeviceMemory memory {};
    ///// What kind of memory do we need? /////

    VkImageMemoryRequirementsInfo2 info {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .image = vkImage,
    };

    VkMemoryRequirements2 memoryRequirements {
      VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };

    vk->GetImageMemoryRequirements2(vkDevice, &info, &memoryRequirements);

    VkMemoryWin32HandlePropertiesKHR handleProperties {
      .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
    };
    winrt::check_hresult(vk->GetMemoryWin32HandlePropertiesKHR(
      vkDevice,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT,
      sharedHandle,
      &handleProperties));

    ///// Which memory areas meet those requirements? /////

    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vk->GetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &memoryProperties);

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
      .image = vkImage,
    };

    VkMemoryAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &dedicatedAllocInfo,
      .allocationSize = memoryRequirements.memoryRequirements.size,
      .memoryTypeIndex = *memoryTypeIndex,
    };

    check_vkresult(
      vk->AllocateMemory(vkDevice, &allocInfo, vkAllocator, &memory));

    VkBindImageMemoryInfo bindImageInfo {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .image = vkImage,
      .memory = memory,
    };

    check_vkresult(vk->BindImageMemory2(vkDevice, 1, &bindImageInfo));
  }

  {
    std::vector<ID3D11Texture2D*> rawPointers;
    rawPointers.reserve(textureCount);
    for (const auto& texture: d3d11Textures) {
      rawPointers.push_back(texture.get());
    }
    mRendererResources = std::make_unique<RendererSwapchainResources>(
      d3d11DeviceResources,
      DXGI_FORMAT_B8G8R8A8_UNORM,
      rawPointers.size(),
      rawPointers.data());
  }

  {
    VkFenceCreateInfo createInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    mVKCompletionFence
      = vk->make_unique<VkFence>(vkDevice, &createInfo, vkAllocator);
  }

  // 1/3: Create a D3D11 fence...
  winrt::check_hresult(d3d11Device->CreateFence(
    0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(mD3D11InteropFence.put())));

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
    mVKInteropSemaphore
      = vk->make_unique<VkSemaphore>(vkDevice, &createInfo, vkAllocator);
  }

  // 3/3: tie them together
  winrt::check_hresult(mD3D11InteropFence->CreateSharedHandle(
    nullptr, GENERIC_ALL, nullptr, mD3D11InteropFenceHandle.put()));
  {
    VkImportSemaphoreWin32HandleInfoKHR handleInfo {
      .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .semaphore = mVKInteropSemaphore.get(),
      .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT,
      .handle = mD3D11InteropFenceHandle.get(),
    };
    check_vkresult(vk->ImportSemaphoreWin32HandleKHR(vkDevice, &handleInfo));
  }

  dprint("Finished initializing interop");
}

OpenXRVulkanKneeboard::~OpenXRVulkanKneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRVulkanKneeboard()");
}

winrt::com_ptr<ID3D11Device> OpenXRVulkanKneeboard::GetD3D11Device() {
  return mDeviceResources->mD3D11Device;
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

  auto dr = mDeviceResources.get();
  auto& sr = mSwapchainResources[swapchain];

  sr.mImages.reserve(imageCount);
  for (const auto& swapchainImage: images) {
    sr.mImages.push_back(swapchainImage.image);
  }
  sr.mVKCommandBuffers.resize(imageCount);

  {
    VkCommandBufferAllocateInfo allocateInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = dr->mVKCommandPool.get(),
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = imageCount,
    };

    check_vkresult(mVK->AllocateCommandBuffers(
      dr->mVKDevice, &allocateInfo, sr.mVKCommandBuffers.data()));
  }

  sr.mInterop = std::make_unique<SwapchainResources::InteropResources>(
    mVK.get(),
    dr->mVKDevice,
    dr->mVKPhysicalDevice,
    dr->mVKAllocator,
    dr->mVKQueueFamilyIndex,
    dr->mRendererDeviceResources.get(),
    imageCount,
    size);
  sr.mSize = size;

  return swapchain;
}

void OpenXRVulkanKneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!mSwapchainResources.contains(swapchain)) {
    return;
  }

  mSwapchainResources.erase(swapchain);
}

void OpenXRVulkanKneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  uint8_t layerCount,
  SHM::LayerSprite* layers) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "OpenXRD3VulkanKneeboard::RenderLayers()");

  auto dr = mDeviceResources.get();
  auto sr = &mSwapchainResources.at(swapchain);

  auto interop = sr->mInterop.get();

  auto rdr = dr->mRendererDeviceResources.get();
  auto rsr = interop->mRendererResources.get();

  namespace R = SHM::D3D11::Renderer;
  R::BeginFrame(rdr, rsr, swapchainTextureIndex);
  R::ClearRenderTargetView(rdr, rsr, swapchainTextureIndex);
  R::Render(
    rdr, rsr, swapchainTextureIndex, mSHM, snapshot, layerCount, layers);
  R::EndFrame(rdr, rsr, swapchainTextureIndex);

  // Signal this once D3D11 work is done, then we pass it as a wait semaphore
  // to vkQueueSubmit
  const auto semaphoreValue = ++interop->mInteropValue;
  dr->mD3D11ImmediateContext->Signal(
    interop->mD3D11InteropFence.get(), semaphoreValue);

  auto interopImage = interop->mVKImages.at(swapchainTextureIndex).get();
  const auto dstImage = sr->mImages.at(swapchainTextureIndex);

  auto vkCommandBuffer = sr->mVKCommandBuffers.at(swapchainTextureIndex);

  VkCommandBufferBeginInfo beginInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  check_vkresult(mVK->ResetCommandBuffer(vkCommandBuffer, 0));
  check_vkresult(mVK->BeginCommandBuffer(vkCommandBuffer, &beginInfo));

  VkImageMemoryBarrier inBarriers[] = {
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = interopImage,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      },
    },
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = dstImage,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      },
    },
  };

  mVK->CmdPipelineBarrier(
    vkCommandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    /* dependency flags = */ {},
    /* memory barrier count = */ 0,
    /* memory barriers = */ nullptr,
    /* buffer barrier count = */ 0,
    /* buffer barriers = */ nullptr,
    /* image barrier count = */ std::size(inBarriers),
    inBarriers);

  std::vector<VkImageCopy> regions;
  regions.reserve(layerCount);
  for (off_t i = 0; i < layerCount; ++i) {
    auto& layer = layers[i];

    // The interop layer is the 'destination', and the swapchain
    // image should be identical, so, we use mDestRect for both
    // source and dest
    const RECT r = layer.mDestRect;

    regions.push_back(
      VkImageCopy {
        .srcSubresource = VkImageSubresourceLayers {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
        .srcOffset = {r.left, r.top, 0},
        .dstSubresource = VkImageSubresourceLayers {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
        .dstOffset = {r.left, r.top, 0},
        .extent = {layer.mDestRect.mSize.mWidth, layer.mDestRect.mSize.mHeight, 1},
      }
    );
  }
  mVK->CmdCopyImage(
    vkCommandBuffer,
    interopImage,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    dstImage,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    regions.size(),
    regions.data());

  VkImageMemoryBarrier outBarriers[] = {
    {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .image = dstImage,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      },
    },
  };

  // Wait for copy to be complete, then...
  mVK->CmdPipelineBarrier(
    vkCommandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    /* dependency flags = */ {},
    /* memory barrier count = */ 0,
    /* memory barriers = */ nullptr,
    /* buffer barrier count = */ 0,
    /* buffer barriers = */ nullptr,
    /* image barrier count = */ std::size(outBarriers),
    outBarriers);
  check_vkresult(mVK->EndCommandBuffer(vkCommandBuffer));

  VkTimelineSemaphoreSubmitInfoKHR timelineSubmitInfo {
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .waitSemaphoreValueCount = 1,
    .pWaitSemaphoreValues = &semaphoreValue,
  };
  VkPipelineStageFlags semaphoreStages {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};
  VkSemaphore waitSemaphores[] = {interop->mVKInteropSemaphore.get()};
  VkSubmitInfo submitInfo {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &timelineSubmitInfo,
    .waitSemaphoreCount = std::size(waitSemaphores),
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = &semaphoreStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &vkCommandBuffer,
  };

  VkFence fences[] = {interop->mVKCompletionFence.get()};
  check_vkresult(mVK->ResetFences(dr->mVKDevice, std::size(fences), fences));
  check_vkresult(mVK->QueueSubmit(
    dr->mVKQueue, 1, &submitInfo, interop->mVKCompletionFence.get()));

  TraceLoggingWriteStop(activity, "OpenXRD3VulkanKneeboard::RenderLayers()");
}// namespace OpenKneeboard

SHM::CachedReader* OpenXRVulkanKneeboard::GetSHM() {
  return &mSHM;
}

}// namespace OpenKneeboard
