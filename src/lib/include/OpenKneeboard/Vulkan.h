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

#include <OpenKneeboard/Pixels.h>
#include <OpenKneeboard/Vulkan/Dispatch.h>
#include <OpenKneeboard/Vulkan/functional.h>
#include <OpenKneeboard/Vulkan/impl/OPENKNEEBOARD_VK_FUNCS.h>
#include <OpenKneeboard/Vulkan/smart-pointers.h>
#include <OpenKneeboard/Vulkan/vkresult.h>

#include <OpenKneeboard/dprint.h>

#include <vulkan/vulkan.h>

namespace OpenKneeboard::Vulkan {

class Color : public std::array<float, 4> {};

class Opacity : public Color {
 public:
  Opacity() = delete;
  Opacity(float opacity) : Color {opacity, opacity, opacity, opacity} {
  }
};

namespace Colors {
constexpr Color White {1, 1, 1, 1};
constexpr Color Transparent {0, 0, 0, 0};
};// namespace Colors

std::optional<uint32_t> FindMemoryType(
  Dispatch*,
  VkPhysicalDevice,
  uint32_t filter,
  VkMemoryPropertyFlags flags);

class SpriteBatch {
 public:
  SpriteBatch() = delete;
  SpriteBatch(
    Dispatch* dispatch,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VkAllocationCallbacks* allocator,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex);
  ~SpriteBatch();

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
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };

 private:
  Dispatch* mVK {nullptr};
  VkPhysicalDevice mPhysicalDevice {nullptr};
  VkDevice mDevice {nullptr};
  const VkAllocationCallbacks* mAllocator {nullptr};
  uint32_t mQueueFamilyIndex {~(0ui32)};
  VkQueue mQueue {nullptr};

  unique_ptr<VkShaderModule> mPixelShader;
  unique_ptr<VkShaderModule> mVertexShader;

  unique_ptr<VkCommandPool> mCommandPool;
  VkCommandBuffer mCommandBuffer {nullptr};

  PixelSize mDestSize;

  struct Vertex {
    class Position : public std::array<float, 4> {
     public:
      constexpr Position(const std::array<float, 2>& pos_2d)
        : std::array<float, 4> {pos_2d[0], pos_2d[0], 0, 1} {
      }

      constexpr Position(float x, float y) : std::array<float, 4> {x, y, 0, 1} {
      }
    };
    alignas(4) uint32_t mTextureIndex {~(0ui32)};
    alignas(16) Color mColor {};
    alignas(8) std::array<float, 2> mTexCoord;
    alignas(16) Position mPosition;

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

  bool mBetweenBeginAndEnd {false};
  std::vector<Sprite> mSprites;

  template <class T>
  struct Buffer {
    unique_ptr<VkBuffer> mBuffer;
    unique_ptr<VkDeviceMemory> mMemory;
    MemoryMapping<T> mMapping;
  };

  Buffer<Vertex> mVertexBuffer;

  void InitializeVertexBuffer();

  struct DescriptorSet : public Buffer<std::byte> {
    unique_ptr<VkDescriptorSetLayout> mLayout;
    VkDeviceSize mDescriptorSize {};
    VkDeviceSize mOffset {};
    unique_ptr<VkDescriptorPool> mDescriptorPool {};
    VkDescriptorSet mDescriptorSet {};
  };

  void CreateDescriptorSet(
    const VkDescriptorSetLayoutBinding*,
    const VkDescriptorSetLayoutCreateInfo*,
    VkDeviceSize descriptorCount,
    VkMemoryPropertyFlags,
    VkDescriptorPoolCreateFlags,
    DescriptorSet*,
    const std::source_location& loc = std::source_location::current());

  unique_ptr<VkSampler> mSampler;
  DescriptorSet mSamplerDescriptorSet;

  void InitializeSampler();

  DescriptorSet mSourceDescriptorSet;

  void InitializeSourceDescriptorSet();
};

}// namespace OpenKneeboard::Vulkan