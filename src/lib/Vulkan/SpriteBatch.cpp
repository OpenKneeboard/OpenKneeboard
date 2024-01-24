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

#include <OpenKneeboard/Shaders/SPIRV/SpriteBatch.h>
#include <OpenKneeboard/Vulkan.h>

#include <OpenKneeboard/tracing.h>

namespace OpenKneeboard::Vulkan {

SpriteBatch::SpriteBatch(
  Dispatch* dispatch,
  VkPhysicalDevice physicalDevice,
  VkDevice device,
  const VkAllocationCallbacks* allocator,
  uint32_t queueFamilyIndex,
  uint32_t queue)
  : mVK(dispatch),
    mPhysicalDevice(physicalDevice),
    mDevice(device),
    mAllocator(allocator),
    mQueueFamilyIndex(queueFamilyIndex) {
  OPENKNEEBOARD_TraceLoggingScope("SpriteBatch::SpriteBatch()");

  mVK->GetDeviceQueue(device, queueFamilyIndex, queue, &mQueue);

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

  this->CreateVertexBuffer();
  this->CreateSampler();
  this->CreateSourceDescriptorSet();

  this->CreatePipeline();
}

void SpriteBatch::CreatePipeline() {
  {
    VkDescriptorSetLayout descriptorSetLayouts[] {
      mSamplerDescriptorSet.mLayout.get(),
      mSourceDescriptorSet.mLayout.get(),
    };
    VkPipelineLayoutCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = std::size(descriptorSetLayouts),
      .pSetLayouts = descriptorSetLayouts,
    };
    mPipelineLayout
      = mVK->make_unique<VkPipelineLayout>(mDevice, &createInfo, mAllocator);
  }

  auto vertexDescription = Vertex::GetBindingDescription();
  auto vertexAttributes = Vertex::GetAttributeDescription();

  VkPipelineVertexInputStateCreateInfo vertex {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &vertexDescription,
    .vertexAttributeDescriptionCount = vertexAttributes.size(),
    .pVertexAttributeDescriptions = vertexAttributes.data(),
  };
  VkPipelineInputAssemblyStateCreateInfo inputAssembly {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
  };
  VkPipelineRasterizationStateCreateInfo rasterization {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
  };
  VkPipelineColorBlendAttachmentState colorBlendAttachmentState {
    .blendEnable = VK_FALSE,
    .colorWriteMask = 0xf,
  };
  VkPipelineColorBlendStateCreateInfo colorBlend {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &colorBlendAttachmentState,
  };
  VkPipelineDepthStencilStateCreateInfo depthStencil {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .minDepthBounds = 0,
    .maxDepthBounds = 1,
  };
  VkPipelineViewportStateCreateInfo viewport {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };
  VkPipelineMultisampleStateCreateInfo multiplesample {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineShaderStageCreateInfo shaderStages[] {
    VkPipelineShaderStageCreateInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = mVertexShader.get(),
      .pName = "SpriteVertexShader",
    },
    VkPipelineShaderStageCreateInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = mPixelShader.get(),
      .pName = "SpritePixelShader",
    },
  };

  VkFormat colorFormats[] {VK_FORMAT_B8G8R8A8_UNORM};

  VkPipelineRenderingCreateInfoKHR renderingCreateInfo {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = colorFormats,
  };

  VkDynamicState dynamicStates[] {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamicState {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = std::size(dynamicStates),
    .pDynamicStates = dynamicStates,
  };

  VkGraphicsPipelineCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &renderingCreateInfo,
    .stageCount = std::size(shaderStages),
    .pStages = shaderStages,
    .pVertexInputState = &vertex,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState = &viewport,
    .pMultisampleState = &multiplesample,
    .pDepthStencilState = &depthStencil,
    .pColorBlendState = &colorBlend,
    .pDynamicState = &dynamicState,
    .layout = mPipelineLayout.get(),
  };

  mPipeline = mVK->make_unique_graphics_pipeline(
    mDevice, VK_NULL_HANDLE, &createInfo, mAllocator);
}

void SpriteBatch::CreateVertexBuffer() {
  VkBufferCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = sizeof(Vertex) * MaxVerticesPerBatch,
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  mVertexBuffer.mBuffer
    = mVK->make_unique<VkBuffer>(mDevice, &createInfo, mAllocator);

  VkMemoryRequirements requirements;
  mVK->GetBufferMemoryRequirements(
    mDevice, mVertexBuffer.mBuffer.get(), &requirements);
  const auto memoryType = FindMemoryType(
    mVK,
    mPhysicalDevice,
    requirements.memoryTypeBits,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  if (!memoryType) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL(
      "Couldn't find compatible memory type for vertex buffer");
  }

  VkMemoryAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = *memoryType,
  };

  mVertexBuffer.mMemory
    = mVK->make_unique<VkDeviceMemory>(mDevice, &allocInfo, mAllocator);

  check_vkresult(mVK->BindBufferMemory(
    mDevice, mVertexBuffer.mBuffer.get(), mVertexBuffer.mMemory.get(), 0));

  mVertexBuffer.mMapping = mVK->MemoryMapping<Vertex>(
    mDevice, mVertexBuffer.mMemory.get(), 0, requirements.size, 0);
}

SpriteBatch::~SpriteBatch() {
  OPENKNEEBOARD_TraceLoggingWrite("SpriteBatch::~SpriteBatch()");
  if (mTarget) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Closing spritebatch without calling End()");
  }
}

void SpriteBatch::Begin(
  VkImageView target,
  const PixelSize& targetSize,
  const std::source_location& caller) {
  if (mTarget) [[unlikely]] {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      caller, "Begin() called but already in progress; did you call End()?");
  }
  mTarget = target;
  mTargetSize = targetSize;
}

void SpriteBatch::Draw(
  VkImageView source,
  const PixelSize& sourceSize,
  const PixelRect& sourceRect,
  const PixelRect& destRect,
  const Color& color,
  const std::source_location& loc) {
  if (!mTarget) [[unlikely]] {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      loc, "Calling Draw() without Begin()");
  }

  mSprites.push_back({source, sourceSize, sourceRect, destRect, color});
}

void SpriteBatch::Clear(Color color, const std::source_location& caller) {
  if (!mTarget) [[unlikely]] {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      caller, "Calling Clear() without Begin()");
  }
  mClearColor = color;
}

void SpriteBatch::End(
  VkFence completionFence,
  const std::source_location& loc) {
  if (!mTarget) [[unlikely]] {
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
  vertices.reserve(mSprites.size() * VerticesPerSprite);

  for (const auto& sprite: mSprites) {
    if (!sourceIndices.contains(sprite.mSource)) {
      sources.push_back(sprite.mSource);
      sourceIndices[sprite.mSource] = sources.size() - 1;
    }

    // Calculate the four source corners in texture coordinates (0..1)
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

    // Destination coordinates in real 3d coordinates
    const auto tl = sprite.mDestRect.TopLeft().StaticCast<Position, float>();
    const auto br
      = sprite.mDestRect.BottomRight().StaticCast<Position, float>();
    const Position tr {br[0], tl[1]};
    const Position bl {tl[0], br[1]};

    const auto sourceIndex = sourceIndices.at(sprite.mSource);
    auto makeVertex = [=](const TexCoord& tc, const Position& pos) {
      return Vertex {
        .mTextureIndex = sourceIndices.at(sprite.mSource),
        .mColor = sprite.mColor,
        .mTexCoord = tc,
        .mPosition = pos,
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

  const size_t verticesByteSize = sizeof(vertices[0]) * vertices.size();
  memcpy(mVertexBuffer.mMapping.get(), vertices.data(), verticesByteSize);
  // FIXME: also fill descriptor buffers

  VkMappedMemoryRange memoryRange {
    .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
    .memory = mVertexBuffer.mMemory.get(),
    .offset = 0,
    .size = verticesByteSize,
  };
  check_vkresult(mVK->FlushMappedMemoryRanges(mDevice, 1, &memoryRange));

  check_vkresult(mVK->ResetCommandBuffer(mCommandBuffer, 0));

  {
    // TODO: remove ONE_TIME_SUBMIT_BIT and re-use this buffer.
    VkCommandBufferBeginInfo beginInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    check_vkresult(mVK->BeginCommandBuffer(mCommandBuffer, &beginInfo));
  }

  {
    VkRenderingAttachmentInfoKHR colorAttachmentInfo {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .imageView = mTarget,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfoKHR renderInfo {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .renderArea = {{0, 0}, {mTargetSize.mWidth, mTargetSize.mHeight}},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentInfo,
    };
    mVK->CmdBeginRenderingKHR(mCommandBuffer, &renderInfo);
  }

  const VkViewport viewport {
    0, 0, mTargetSize.GetWidth<float>(), mTargetSize.GetHeight<float>(), 0, 1};
  mVK->CmdSetViewport(mCommandBuffer, 0, 1, &viewport);
  const VkRect2D scissorRect = {
    {0, 0},
    {mTargetSize.mWidth, mTargetSize.mHeight},
  };
  mVK->CmdSetScissor(mCommandBuffer, 0, 1, &scissorRect);
  mVK->CmdBindPipeline(
    mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline.get());

  VkBuffer vertexBuffers[] {mVertexBuffer.mBuffer.get()};
  VkDeviceSize vertexBufferOffsets[] {0};
  static_assert(std::size(vertexBuffers) == std::size(vertexBufferOffsets));
  mVK->CmdBindVertexBuffers(
    mCommandBuffer,
    0,
    std::size(vertexBuffers),
    vertexBuffers,
    vertexBufferOffsets);

  {
    // TODO: once we stop one-shotting the command buffer, we can update the
    // source infos after bind
    VkDescriptorImageInfo samplerInfo {
      .sampler = mSampler.get(),
    };
    std::vector<VkDescriptorImageInfo> sourceInfos;
    sourceInfos.reserve(sources.size());
    for (const auto& source: sources) {
      sourceInfos.push_back({.imageView = source});
    }

    VkWriteDescriptorSet writeDescriptors[] {
      VkWriteDescriptorSet {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = mSamplerDescriptorSet.mDescriptorSet,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &samplerInfo,
      },
      VkWriteDescriptorSet {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = mSourceDescriptorSet.mDescriptorSet,
        .descriptorCount = static_cast<uint32_t>(sourceInfos.size()),
        .pImageInfo = sourceInfos.data(),
      },
    };
    mVK->UpdateDescriptorSets(
      mDevice, std::size(writeDescriptors), writeDescriptors, 0, nullptr);
  }

  VkDescriptorSet descriptors[] {
    mSourceDescriptorSet.mDescriptorSet,
    mSamplerDescriptorSet.mDescriptorSet,
  };

  mVK->CmdBindDescriptorSets(
    mCommandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    mPipelineLayout.get(),
    0,
    std::size(descriptors),
    descriptors,
    0,
    nullptr);

  if (mClearColor) {
    VkClearAttachment clear {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .colorAttachment = 0,
      .clearValue = {
        .color = {
          .float32 = {
            (*mClearColor)[0],
            (*mClearColor)[1],
            (*mClearColor)[2],
            (*mClearColor)[3],
          },
        },
      },
    };
  }

  mVK->CmdDrawIndexed(mCommandBuffer, vertices.size(), 1, 0, 0, 0);

  mVK->CmdEndRenderingKHR(mCommandBuffer);

  check_vkresult(mVK->EndCommandBuffer(mCommandBuffer));

  {
    // TODO: do we need to wait for anything?
    VkSubmitInfo submitInfo {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &mCommandBuffer,
    };
    check_vkresult(mVK->QueueSubmit(mQueue, 1, &submitInfo, completionFence));
  }

  mSprites.clear();
  mTarget = nullptr;
  mClearColor = {};
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
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = offsetof(Vertex, mPosition),
    },
  };
}

static VkDeviceSize aligned_size(VkDeviceSize size, VkDeviceSize alignment) {
  const auto maxPad = alignment - 1;
  return (size + maxPad) & ~maxPad;
}

void SpriteBatch::CreateSampler() {
  {
    VkSamplerCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .maxLod = VK_LOD_CLAMP_NONE,
    };
    mSampler = mVK->make_unique<VkSampler>(mDevice, &createInfo, mAllocator);
  }

  VkSampler samplers[] = {mSampler.get()};
  VkDescriptorSetLayoutBinding layoutBinding {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = samplers,
  };

  VkDescriptorSetLayoutCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings = &layoutBinding,
  };

  this->CreateDescriptorSet(
    &layoutBinding,
    &createInfo,
    1,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    0,
    &mSamplerDescriptorSet);
}

void SpriteBatch::CreateSourceDescriptorSet() {
  VkSampler samplers[] = {mSampler.get()};
  VkDescriptorSetLayoutBinding layoutBinding {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = samplers,
  };

  VkDescriptorBindingFlags bindingFlags {
    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
    | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT
    | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT
    | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT};
  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsCreateInfo {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount = 1,
    .pBindingFlags = &bindingFlags,
  };

  VkDescriptorSetLayoutCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = &bindingFlagsCreateInfo,
    .bindingCount = 1,
    .pBindings = &layoutBinding,
  };

  this->CreateDescriptorSet(
    &layoutBinding,
    &createInfo,
    MaxSpritesPerBatch,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,

    &mSourceDescriptorSet);
}

void SpriteBatch::CreateDescriptorSet(
  const VkDescriptorSetLayoutBinding* layoutBinding,
  const VkDescriptorSetLayoutCreateInfo* layoutCreateInfo,
  VkDeviceSize descriptorCount,
  VkMemoryPropertyFlags memoryPropertyFlags,
  VkDescriptorPoolCreateFlags poolCreateFlags,
  DescriptorSet* ret,
  const std::source_location& loc) {
  ret->mLayout = mVK->make_unique<VkDescriptorSetLayout>(
    mDevice, layoutCreateInfo, mAllocator);

  VkPhysicalDeviceDescriptorBufferPropertiesEXT bufferProperties {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
  };
  VkPhysicalDeviceProperties2KHR deviceProperties {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
    .pNext = &bufferProperties,
  };

  mVK->GetPhysicalDeviceProperties2KHR(mPhysicalDevice, &deviceProperties);

  mVK->GetDescriptorSetLayoutSizeEXT(
    mDevice, ret->mLayout.get(), &ret->mDescriptorSize);
  ret->mDescriptorSize = aligned_size(
    ret->mDescriptorSize, bufferProperties.descriptorBufferOffsetAlignment);
  mVK->GetDescriptorSetLayoutBindingOffsetEXT(
    mDevice, ret->mLayout.get(), 0, &ret->mOffset);

  {
    VkBufferCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = ret->mDescriptorSize * descriptorCount,
      .usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
        | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &mQueueFamilyIndex,
    };
    ret->mBuffer = mVK->make_unique<VkBuffer>(mDevice, &createInfo, mAllocator);
  }

  VkMemoryRequirements requirements;
  mVK->GetBufferMemoryRequirements(mDevice, ret->mBuffer.get(), &requirements);
  const auto memoryType = FindMemoryType(
    mVK, mPhysicalDevice, requirements.memoryTypeBits, memoryPropertyFlags);
  if (!memoryType) [[unlikely]] {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      loc, "Couldn't find compatible memory type for descriptor buffer");
  }

  {
    VkMemoryAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = requirements.size,
      .memoryTypeIndex = *memoryType,
    };

    ret->mMemory
      = mVK->make_unique<VkDeviceMemory>(mDevice, &allocInfo, mAllocator);
  }

  check_vkresult(
    mVK->BindBufferMemory(mDevice, ret->mBuffer.get(), ret->mMemory.get(), 0));

  ret->mMapping = mVK->MemoryMapping<std::byte>(
    mDevice, ret->mMemory.get(), ret->mOffset, requirements.size, 0);

  {
    VkDescriptorPoolSize poolSize {
      .type = layoutBinding->descriptorType,
      .descriptorCount = static_cast<uint32_t>(descriptorCount),
    };
    VkDescriptorPoolCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = poolCreateFlags,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize,
    };
    ret->mDescriptorPool
      = mVK->make_unique<VkDescriptorPool>(mDevice, &createInfo, mAllocator);
  }

  VkDescriptorSetLayout layouts[] {ret->mLayout.get()};
  VkDescriptorSetAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = ret->mDescriptorPool.get(),
    .descriptorSetCount = std::size(layouts),
    .pSetLayouts = layouts,
  };

  check_vkresult(
    mVK->AllocateDescriptorSets(mDevice, &allocInfo, &ret->mDescriptorSet));
}

}// namespace OpenKneeboard::Vulkan