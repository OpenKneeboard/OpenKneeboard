// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "Dispatch.hpp"
#include "colors.hpp"
#include "smart-pointers.hpp"

#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/Shaders/SpriteBatch.hpp>
#include <OpenKneeboard/Vulkan/ExtendedCreateInfo.hpp>

namespace OpenKneeboard::Vulkan {

class SpriteBatch {
 public:
  struct InstanceCreateInfo : ExtendedCreateInfo<VkInstanceCreateInfo> {
    InstanceCreateInfo(const VkInstanceCreateInfo& base);
  };

  struct DeviceCreateInfo : ExtendedCreateInfo<VkDeviceCreateInfo> {
    DeviceCreateInfo(const VkDeviceCreateInfo& base);

   private:
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT mDescriptorIndexingFeatures {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
    VkPhysicalDeviceDynamicRenderingFeaturesKHR mDynamicRenderingFeatures {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};
  };

  SpriteBatch() = delete;
  SpriteBatch(
    Dispatch* dispatch,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VkAllocationCallbacks* allocator,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex);
  ~SpriteBatch();

  /** Start a spritebatch.
   *
   * `target` MUST have the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout.
   */
  void Begin(
    VkCommandBuffer,
    VkImageView target,
    const PixelSize& size,
    const std::source_location& loc = std::source_location::current());

  void Clear(
    Color color = Colors::Transparent,
    const std::source_location& loc = std::source_location::current());

  void Draw(
    VkImageView source,
    const PixelSize& sourceSize,
    const PixelRect& sourceRect,
    const PixelRect& destRect,
    const Color& color = Colors::White,
    const std::source_location& loc = std::source_location::current());

  void End(const std::source_location& loc = std::source_location::current());

  static constexpr std::string_view REQUIRED_DEVICE_EXTENSIONS[] {
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
    VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_MAINTENANCE2_EXTENSION_NAME,
    VK_KHR_MAINTENANCE3_EXTENSION_NAME,
    VK_KHR_MULTIVIEW_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
  };

  static constexpr std::string_view REQUIRED_INSTANCE_EXTENSIONS[] {
    VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };

 private:
  Dispatch* mVK {nullptr};
  VkPhysicalDevice mPhysicalDevice {nullptr};
  VkDevice mDevice {nullptr};
  const VkAllocationCallbacks* mAllocator {nullptr};
  uint32_t mQueueFamilyIndex {~(0ui32)};
  VkQueue mQueue {nullptr};

  unique_vk<VkPipelineLayout> mPipelineLayout;
  unique_vk<VkPipeline> mPipeline;

  VkCommandBuffer mCommandBuffer {nullptr};
  VkImageView mTarget {nullptr};
  PixelSize mTargetDimensions;
  std::optional<Color> mClearColor;

  unique_vk<VkShaderModule> mPixelShader;
  unique_vk<VkShaderModule> mVertexShader;

  static VkVertexInputBindingDescription GetVertexBindingDescription();
  static std::array<VkVertexInputAttributeDescription, 4>
  GetVertexAttributeDescription();

  struct Sprite {
    VkImageView mSource {nullptr};
    PixelSize mSourceSize;
    PixelRect mSourceRect;
    PixelRect mDestRect;
    Color mColor;
  };

  static constexpr auto MaxSpritesPerBatch
    = Shaders::SpriteBatch::MaxSpritesPerBatch;
  static constexpr auto VerticesPerSprite
    = Shaders::SpriteBatch::VerticesPerSprite;
  static constexpr auto MaxVerticesPerBatch
    = Shaders::SpriteBatch::MaxVerticesPerBatch;

  std::vector<Sprite> mSprites;

  void CreatePipeline();

  template <class T>
  struct Buffer {
    unique_vk<VkBuffer> mBuffer;
    unique_vk<VkDeviceMemory> mMemory;
    MemoryMapping<T> mMapping;

    Buffer() = default;

    inline Buffer(Buffer<T>&& other) {
      mBuffer = std::move(other.mBuffer);
      mMemory = std::move(other.mMemory);
      mMapping = std::move(other.mMapping);
    }

    Buffer& operator=(Buffer<T>&&) = default;
  };

  using UniformBuffer = Shaders::SpriteBatch::UniformBuffer;
  using Vertex = Shaders::SpriteBatch::Vertex;

  Buffer<Vertex> mVertexBuffer;
  Buffer<UniformBuffer> mUniformBuffer;

  template <class T>
  Buffer<T> CreateBuffer(VkBufferUsageFlags, VkDeviceSize size);

  void CreateUniformBuffer();
  void CreateVertexBuffer();

  unique_vk<VkSampler> mSampler;

  struct DescriptorSet {
    unique_vk<VkDescriptorSetLayout> mLayout;
    unique_vk<VkDescriptorPool> mDescriptorPool;
    VkDescriptorSet mDescriptorSet {VK_NULL_HANDLE};
  };
  DescriptorSet mDescriptorSet;

  void CreateSampler();
  void CreateDescriptorSet();
};

}// namespace OpenKneeboard::Vulkan