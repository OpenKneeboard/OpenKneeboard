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

#include <OpenKneeboard/Shaders/SPIRV/SpriteBatch.h>
#include <OpenKneeboard/Vulkan.h>

#include <OpenKneeboard/tracing.h>

#include <unordered_map>

namespace OpenKneeboard::Vulkan {

Dispatch::Dispatch(
  VkInstance instance,
  PFN_vkGetInstanceProcAddr getInstanceProcAddr) {
#define IT(vkfun) \
  this->##vkfun = reinterpret_cast<PFN_vk##vkfun>( \
    getInstanceProcAddr(instance, "vk" #vkfun));
  OPENKNEEBOARD_VK_FUNCS
#undef IT
}

SpriteBatch::SpriteBatch(
  Dispatch* dispatch,
  VkDevice device,
  const VkAllocationCallbacks* allocator,
  uint32_t queueFamilyIndex) {
  OPENKNEEBOARD_TraceLoggingScope("SpriteBatch::SpriteBatch()");

  namespace Shaders = Shaders::SPIRV::SpriteBatch;

  {
    VkShaderModuleCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = Shaders::PS.size(),
      .pCode = reinterpret_cast<const uint32_t*>(Shaders::PS.data()),
    };
    mPixelShader
      = dispatch->make_unique<VkShaderModule>(device, &createInfo, allocator);
  }

  {
    VkShaderModuleCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = Shaders::VS.size(),
      .pCode = reinterpret_cast<const uint32_t*>(Shaders::VS.data()),
    };
    mVertexShader
      = dispatch->make_unique<VkShaderModule>(device, &createInfo, allocator);
  }

  {
    VkCommandPoolCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndex,
    };
    mCommandPool
      = dispatch->make_unique<VkCommandPool>(device, &createInfo, allocator);
  }

  {
    VkCommandBufferAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = mCommandPool.get(),
      .commandBufferCount = 1,
    };
    check_vkresult(
      dispatch->AllocateCommandBuffers(device, &allocInfo, &mCommandBuffer));
  }
}

SpriteBatch::~SpriteBatch() {
  OPENKNEEBOARD_TraceLoggingWrite("SpriteBatch::~SpriteBatch()");
}

void SpriteBatch::Draw(
  VkImageView source,
  const PixelSize& sourceSize,
  const PixelRect& sourceRect,
  const PixelRect& destRect,
  const Color& color,
  const std::source_location& loc) {
  if (!mBetweenBeginAndEnd) [[unlikely]] {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      loc, "Calling Draw() without Begin()");
  }

  mSprites.push_back({source, sourceSize, sourceRect, destRect, color});
}

void SpriteBatch::End(const std::source_location& loc) {
  if (!mBetweenBeginAndEnd) [[unlikely]] {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      loc, "Calling End() without Begin()");
  }

  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity,
    "Vulkan::SpriteBatch::End",
    TraceLoggingValue(mSprites.size(), "SpriteCount"));
  if (mSprites.empty()) {
    return;
  }

  std::vector<VkImageView> sources;
  std::unordered_map<VkImageView, uint32_t> sourceIndices;
  std::vector<Vertex> vertices;
  vertices.reserve(mSprites.size() * 6);

  for (const auto& sprite: mSprites) {
    if (!sourceIndices.contains(sprite.mSource)) {
      sources.push_back(sprite.mSource);
      sourceIndices[sprite.mSource] = sources.size() - 1;
    }

    using TexCoord = std::array<float, 2>;
    TexCoord tctl;
    TexCoord tctr;
    TexCoord tcbl;
    TexCoord tcbr;

    {
      const auto& ss = sprite.mSourceSize;

      const auto tl = sprite.mSourceRect.TopLeft();
      const auto br = sprite.mSourceRect.BottomRight();

      const auto left = tl.GetX<float>() / ss.mWidth;
      const auto top = tl.GetY<float>() / ss.mHeight;
      const auto right = br.GetX<float>() / ss.mWidth;
      const auto bottom = br.GetY<float>() / ss.mHeight;

      tctl = {left, top};
      tctr = {right, top};
      tcbl = {left, bottom};
      tcbr = {right, bottom};
    };

    using Position = Vertex::Position;

    const auto tl = sprite.mDestRect.TopLeft().StaticCast<Position, float>();
    const auto br
      = sprite.mDestRect.BottomRight().StaticCast<Position, float>();
    const Position tr {br[0], tl[1]};
    const Position bl {tl[0], br[1]};

    const auto sourceIndex = sourceIndices.at(sprite.mSource);
    auto makeVertex = [=](const TexCoord& tc, const Position& dp) {
      return Vertex {
        .mTextureIndex = sourceIndices.at(sprite.mSource),
        .mColor = sprite.mColor,
        .mTexCoord = tc,
        .mPosition = dp,
      };
    };

    // First triangle: excludes top right
    vertices.push_back(makeVertex(tcbl, bl));
    vertices.push_back(makeVertex(tctl, tl));
    vertices.push_back(makeVertex(tcbr, br));

    // First triangle: excludes bottom left
    vertices.push_back(makeVertex(tctl, tl));
    vertices.push_back(makeVertex(tctr, tr));
    vertices.push_back(makeVertex(tcbr, br));
  }

  mSprites.clear();
  mBetweenBeginAndEnd = false;
}

VkVertexInputBindingDescription SpriteBatch::Vertex::GetBindingDescription() {
  return VkVertexInputBindingDescription {
    .stride = sizeof(Vertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
}
std::array<VkVertexInputAttributeDescription, 4>
SpriteBatch::Vertex::GetAttributeDescription() {
  return {
    VkVertexInputAttributeDescription {
      .location = 0,
      .format = VK_FORMAT_R32_UINT,
      .offset = offsetof(Vertex, mTextureIndex),
    },
    VkVertexInputAttributeDescription {
      .location = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = offsetof(Vertex, mColor),
    },
    VkVertexInputAttributeDescription {
      .location = 2,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(Vertex, mTexCoord),
    },
    VkVertexInputAttributeDescription {
      .location = 3,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(Vertex, mPosition),
    },
  };
}

}// namespace OpenKneeboard::Vulkan