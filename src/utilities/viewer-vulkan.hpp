// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "viewer.hpp"

#include <OpenKneeboard/SHM/Vulkan.hpp>
#include <OpenKneeboard/Vulkan/SpriteBatch.hpp>

#include <OpenKneeboard/handles.hpp>

#include <cinttypes>

namespace OpenKneeboard::Viewer {
class VulkanRenderer final : public Renderer {
 public:
  VulkanRenderer() = delete;
  VulkanRenderer(uint64_t luid);
  virtual ~VulkanRenderer();

  SHM::Reader& GetSHM() override;
  std::wstring_view GetName() const noexcept override;

  void Initialize(uint8_t swapchainLength) override;

  void SaveToDDSFile(SHM::Frame frame, const std::filesystem::path&) override;

  uint64_t Render(
    SHM::Frame,
    const PixelRect& sourceRect,
    HANDLE destTexture,
    const PixelSize& destTextureDimensions,
    const PixelRect& destRect,
    HANDLE fence,
    uint64_t fenceValueIn) override;

 private:
  unique_hmodule mVulkanLoader;

  template <class T>
  using unique_vk = OpenKneeboard::Vulkan::unique_vk<T>;

  unique_vk<VkInstance> mVKInstance;
  std::unique_ptr<OpenKneeboard::Vulkan::Dispatch> mVK;
#ifdef DEBUG
  unique_vk<VkDebugUtilsMessengerEXT> mVKDebugMessenger;
#endif

  VkPhysicalDevice mVKPhysicalDevice {VK_NULL_HANDLE};

  uint32_t mQueueFamilyIndex;

  unique_vk<VkDevice> mDevice;
  VkQueue mQueue;

  unique_vk<VkCommandPool> mCommandPool;
  VkCommandBuffer mCommandBuffer {};
  unique_vk<VkFence> mCompletionFence;

  HANDLE mDestHandle {};
  unique_vk<VkImage> mDestImage;
  unique_vk<VkDeviceMemory> mDestImageMemory;
  unique_vk<VkImageView> mDestImageView;
  PixelSize mDestImageDimensions;

  HANDLE mSemaphoreHandle {};
  unique_vk<VkSemaphore> mSemaphore;

  // Last as it caches some Vulkan resources; as Vulkan doesn't internally use
  // refcounting, we need to make sure these are released before the `unique_vk`
  // above.
  std::unique_ptr<SHM::Vulkan::Reader> mSHM;
  std::unique_ptr<OpenKneeboard::Vulkan::SpriteBatch> mSpriteBatch;

  void InitializeSemaphore(HANDLE);
  void InitializeDest(HANDLE, const PixelSize&);

  void SaveTextureToFile(
    const PixelSize& dimensions,
    VkImage source,
    VkImageLayout layout,
    VkSemaphore waitSemaphore,
    uint64_t waitSemaphoreValue,
    const std::filesystem::path&);
};

}// namespace OpenKneeboard::Viewer