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

#include <OpenKneeboard/SHM/Vulkan.h>

#include <OpenKneeboard/scope_guard.h>

namespace OpenKneeboard::SHM::Vulkan::Renderer {

using OpenKneeboard::Vulkan::check_vkresult;

DeviceResources::DeviceResources(
  OpenKneeboard::Vulkan::Dispatch* vk,
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

void DeviceResources::InitializeD3D11(OpenKneeboard::Vulkan::Dispatch* vk) {
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
  mRendererResources
    = std::make_unique<RendererDeviceResources>(d3d11Device.get());
}

SwapchainResources::SwapchainResources(
  OpenKneeboard::Vulkan::Dispatch* vk,
  DeviceResources* dr,
  const PixelSize& textureSize,
  uint32_t textureCount,
  VkImage* vkImages) noexcept {
  mSize = textureSize;

  std::vector<VkCommandBuffer> commandBuffers;
  commandBuffers.resize(textureCount);

  {
    VkCommandBufferAllocateInfo allocateInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = dr->mVKCommandPool.get(),
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = textureCount,
    };

    check_vkresult(vk->AllocateCommandBuffers(
      dr->mVKDevice, &allocateInfo, commandBuffers.data()));
  }
  for (uint32_t i = 0; i < textureCount; ++i) {
    mBufferResources.push_back({
      .mVKSwapchainImage = vkImages[i],
      .mVKCommandBuffer = commandBuffers.at(i),
    });
  }

  this->InitializeInterop(vk, dr, textureCount, textureSize);
}

void SwapchainResources::InitializeInterop(
  OpenKneeboard::Vulkan::Dispatch* vk,
  DeviceResources* dr,
  uint32_t textureCount,
  const PixelSize& textureSize) noexcept {
  auto d3d11Device = dr->mD3D11Device.get();

  std::vector<winrt::com_ptr<ID3D11Texture2D>> d3d11Textures;
  d3d11Textures.reserve(textureCount);

  for (uint32_t i = 0; i < textureCount; ++i) {
    auto br = &mBufferResources.at(i);

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
      .pQueueFamilyIndices = &dr->mVKQueueFamilyIndex,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    br->mVKInteropImage
      = vk->make_unique<VkImage>(dr->mVKDevice, &createInfo, dr->mVKAllocator);
    auto vkImage = br->mVKInteropImage.get();

    VkDeviceMemory memory {};
    ///// What kind of memory do we need? /////

    VkImageMemoryRequirementsInfo2 info {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .image = vkImage,
    };

    VkMemoryRequirements2 memoryRequirements {
      VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };

    vk->GetImageMemoryRequirements2(dr->mVKDevice, &info, &memoryRequirements);

    VkMemoryWin32HandlePropertiesKHR handleProperties {
      .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
    };
    winrt::check_hresult(vk->GetMemoryWin32HandlePropertiesKHR(
      dr->mVKDevice,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT,
      sharedHandle,
      &handleProperties));

    ///// Which memory areas meet those requirements? /////

    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vk->GetPhysicalDeviceMemoryProperties(
      dr->mVKPhysicalDevice, &memoryProperties);

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
      vk->AllocateMemory(dr->mVKDevice, &allocInfo, dr->mVKAllocator, &memory));

    VkBindImageMemoryInfo bindImageInfo {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .image = vkImage,
      .memory = memory,
    };

    check_vkresult(vk->BindImageMemory2(dr->mVKDevice, 1, &bindImageInfo));
  }

  {
    std::vector<ID3D11Texture2D*> rawPointers;
    rawPointers.reserve(textureCount);
    for (const auto& texture: d3d11Textures) {
      rawPointers.push_back(texture.get());
    }
    mRendererResources = std::make_unique<RendererSwapchainResources>(
      dr->mRendererResources.get(),
      DXGI_FORMAT_B8G8R8A8_UNORM,
      rawPointers.size(),
      rawPointers.data());
  }

  {
    VkFenceCreateInfo createInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    mVKCompletionFence
      = vk->make_unique<VkFence>(dr->mVKDevice, &createInfo, dr->mVKAllocator);
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
    mVKInteropSemaphore = vk->make_unique<VkSemaphore>(
      dr->mVKDevice, &createInfo, dr->mVKAllocator);
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
    check_vkresult(
      vk->ImportSemaphoreWin32HandleKHR(dr->mVKDevice, &handleInfo));
  }

  dprint("Finished initializing interop");
}

}// namespace OpenKneeboard::SHM::Vulkan::Renderer