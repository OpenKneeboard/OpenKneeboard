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
#pragma once

#include "Dispatch.h"
#include "colors.h"
#include "smart-pointers.h"

#include <OpenKneeboard/Pixels.h>
#include <OpenKneeboard/Vulkan/ExtendedCreateInfo.h>

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

  // MUST MATCH SHADER
  struct BatchData {
    std::array<float, 2> mTargetDimensions;
  };

  // MUST MATCH SHADER
  struct Vertex {
    class Position : public std::array<float, 4> {
     public:
      constexpr Position(const std::array<float, 2>& pos_2d)
        : std::array<float, 4> {pos_2d[0], pos_2d[0], 0, 1} {
      }

      constexpr Position(float x, float y) : std::array<float, 4> {x, y, 0, 1} {
      }
    };
    Position mPosition;
    Color mColor {};
    std::array<float, 2> mTexCoord;
    uint32_t mTextureIndex {~(0ui32)};

    static VkVertexInputBindingDescription GetBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 4>
    GetAttributeDescription();
  };

  struct Sprite {
    VkImageView mSource {nullptr};
    PixelSize mSourceSize;
    PixelRect mSourceRect;
    PixelRect mDestRect;
    Color mColor;
  };

  constexpr static uint8_t VerticesPerSprite = 6;
  constexpr static uint8_t MaxSpritesPerBatch = 16;
  constexpr static uint8_t MaxVerticesPerBatch
    = VerticesPerSprite * MaxSpritesPerBatch;

  std::vector<Sprite> mSprites;

  void CreatePipeline();

  template <class T>
  struct Buffer {
    unique_vk<VkBuffer> mBuffer;
    unique_vk<VkDeviceMemory> mMemory;
    MemoryMapping<T> mMapping;
  };

  Buffer<Vertex> mVertexBuffer;

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