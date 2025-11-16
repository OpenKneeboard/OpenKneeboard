// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/Shaders/SpriteBatch/SPIRV.hpp>
#include <OpenKneeboard/Vulkan.hpp>

#include <OpenKneeboard/tracing.hpp>

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

  namespace Shaders = Shaders::SpriteBatch::SPIRV;

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

  this->CreateUniformBuffer();
  this->CreateVertexBuffer();
  this->CreateSampler();

  this->CreateDescriptorSet();

  this->CreatePipeline();
}

void SpriteBatch::CreatePipeline() {
  {
    VkDescriptorSetLayout descriptorSetLayouts[] {
      mDescriptorSet.mLayout.get(),
    };

    VkPipelineLayoutCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = std::size(descriptorSetLayouts),
      .pSetLayouts = descriptorSetLayouts,
    };
    mPipelineLayout
      = mVK->make_unique<VkPipelineLayout>(mDevice, &createInfo, mAllocator);
  }

  auto vertexDescription = GetVertexBindingDescription();
  auto vertexAttributes = GetVertexAttributeDescription();

  VkPipelineVertexInputStateCreateInfo vertex {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &vertexDescription,
    .vertexAttributeDescriptionCount = vertexAttributes.size(),
    .pVertexAttributeDescriptions = vertexAttributes.data(),
  };
  VkPipelineInputAssemblyStateCreateInfo inputAssembly {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  };
  VkPipelineRasterizationStateCreateInfo rasterization {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .lineWidth = 1.0f,
  };
  VkPipelineColorBlendAttachmentState colorBlendAttachmentState {
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
      | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };
  VkPipelineColorBlendStateCreateInfo colorBlend {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &colorBlendAttachmentState,
  };
  VkPipelineViewportStateCreateInfo viewport {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };
  VkPipelineMultisampleStateCreateInfo multiplesample {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .minSampleShading = 1.0f,
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

  VkPipelineDepthStencilStateCreateInfo depthStencil {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
  };

  VkGraphicsPipelineCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &renderingCreateInfo,
    .stageCount = std::size(shaderStages),
    .pStages = shaderStages,
    .pVertexInputState = &vertex,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState = &viewport,
    .pRasterizationState = &rasterization,
    .pMultisampleState = &multiplesample,
    .pDepthStencilState = &depthStencil,
    .pColorBlendState = &colorBlend,
    .pDynamicState = &dynamicState,
    .layout = mPipelineLayout.get(),
    .basePipelineIndex = -1,
  };

  mPipeline = mVK->make_unique_graphics_pipeline(
    mDevice, VK_NULL_HANDLE, &createInfo, mAllocator);
}

template <class T>
SpriteBatch::Buffer<T> SpriteBatch::CreateBuffer(
  VkBufferUsageFlags flags,
  VkDeviceSize size) {
  Buffer<T> ret {};

  VkBufferCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = flags,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  ret.mBuffer = mVK->make_unique<VkBuffer>(mDevice, &createInfo, mAllocator);

  VkMemoryRequirements requirements;
  mVK->GetBufferMemoryRequirements(mDevice, ret.mBuffer.get(), &requirements);
  const auto memoryType = FindMemoryType(
    mVK,
    mPhysicalDevice,
    requirements.memoryTypeBits,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (!memoryType) [[unlikely]] {
    fatal("Couldn't find compatible memory type for vertex buffer");
  }

  VkMemoryAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = *memoryType,
  };

  ret.mMemory
    = mVK->make_unique<VkDeviceMemory>(mDevice, &allocInfo, mAllocator);

  check_vkresult(
    mVK->BindBufferMemory(mDevice, ret.mBuffer.get(), ret.mMemory.get(), 0));

  ret.mMapping = mVK->MemoryMapping<T>(
    mDevice, ret.mMemory.get(), 0, requirements.size, 0);

  return ret;
}

void SpriteBatch::CreateVertexBuffer() {
  mVertexBuffer = this->CreateBuffer<Vertex>(
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(Vertex) * MaxVerticesPerBatch);
}

void SpriteBatch::CreateUniformBuffer() {
  mUniformBuffer = this->CreateBuffer<UniformBuffer>(
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UniformBuffer));
}

SpriteBatch::~SpriteBatch() {
  OPENKNEEBOARD_TraceLoggingWrite("SpriteBatch::~SpriteBatch()");
  if (mTarget) [[unlikely]] {
    fatal("Closing spritebatch without calling End()");
  }
}

void SpriteBatch::Begin(
  VkCommandBuffer commandBuffer,
  VkImageView target,
  const PixelSize& targetSize,
  const std::source_location& caller) {
  if (mTarget) [[unlikely]] {
    fatal(
      caller, "Begin() called but already in progress; did you call End()?");
  }

  mCommandBuffer = commandBuffer;
  mTarget = target;
  mTargetDimensions = targetSize;
}

void SpriteBatch::Draw(
  VkImageView source,
  const PixelSize& sourceSize,
  const PixelRect& sourceRect,
  const PixelRect& destRect,
  const Color& color,
  const std::source_location& loc) {
  if (!mTarget) [[unlikely]] {
    fatal(loc, "Calling Draw() without Begin()");
  }

  mSprites.push_back({
    source,
    sourceSize,
    sourceRect,
    destRect,
    color,
  });
}

void SpriteBatch::Clear(Color color, const std::source_location& caller) {
  if (!mTarget) [[unlikely]] {
    fatal(caller, "Calling Clear() without Begin()");
  }
  mClearColor = color;
}

void SpriteBatch::End(const std::source_location& loc) {
  if (!mTarget) [[unlikely]] {
    fatal(loc, "Calling End() without Begin()");
  }

  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity,
    "Vulkan::SpriteBatch::End",
    TraceLoggingValue(mSprites.size(), "SpriteCount"));
  if (mSprites.empty()) {
    return;
  }

  UniformBuffer batchData {
    .mTargetDimensions
    = {mTargetDimensions.Width<float>(), mTargetDimensions.Height<float>()},
  };

  std::vector<VkImageView> sources;
  std::unordered_map<VkImageView, uint32_t> sourceIndices;
  std::vector<Vertex> vertices;
  vertices.reserve(mSprites.size() * VerticesPerSprite);

  for (const auto& sprite: mSprites) {
    if (!sourceIndices.contains(sprite.mSource)) {
      sources.push_back(sprite.mSource);
      const auto i = sources.size() - 1;
      sourceIndices[sprite.mSource] = i;

      batchData.mSourceDimensions[i] = {
        sprite.mSourceSize.Width<float>(),
        sprite.mSourceSize.Height<float>(),
      };

      batchData.mSourceClamp[i] = {
        (sprite.mSourceRect.Left<float>() + 0.5f) / sprite.mSourceSize.Width(),
        (sprite.mSourceRect.Top<float>() + 0.5f) / sprite.mSourceSize.Height(),
        (sprite.mSourceRect.Right<float>() - 0.5f) / sprite.mSourceSize.Width(),
        (sprite.mSourceRect.Bottom<float>() - 0.5f)
          / sprite.mSourceSize.Height(),
      };
    }

    using TexCoord = std::array<float, 2>;

    const TexCoord srcTL
      = sprite.mSourceRect.TopLeft().StaticCast<float, TexCoord>();
    const TexCoord srcBR
      = sprite.mSourceRect.BottomRight().StaticCast<float, TexCoord>();
    const TexCoord srcBL {srcTL[0], srcBR[1]};
    const TexCoord srcTR {srcBR[0], srcTL[1]};

    using Position = Vertex::Position;

    // Destination coordinates in real 3d coordinates
    const Position dstTL
      = sprite.mDestRect.TopLeft().StaticCast<float, Position>();
    const Position dstBR
      = sprite.mDestRect.BottomRight().StaticCast<float, Position>();
    const Position dstTR {dstBR[0], dstTL[1]};
    const Position dstBL {dstTL[0], dstBR[1]};

    auto makeVertex = [=](const TexCoord& tc, const Position& pos) {
      return Vertex {
        .mPosition = pos,
        .mColor = sprite.mColor,
        .mTexCoord = tc,
        .mTextureIndex = sourceIndices.at(sprite.mSource),
      };
    };

    // A rectangle is two triangles

    // First triangle: excludes top right
    vertices.push_back(makeVertex(srcBL, dstBL));
    vertices.push_back(makeVertex(srcTL, dstTL));
    vertices.push_back(makeVertex(srcBR, dstBR));

    // Second triangle: excludes bottom left
    vertices.push_back(makeVertex(srcTL, dstTL));
    vertices.push_back(makeVertex(srcTR, dstTR));
    vertices.push_back(makeVertex(srcBR, dstBR));
  }

  if (sources.size() > MaxSpritesPerBatch) {
    fatal(
      "OpenKneeboard's Vulkan Spritebatch only supports up to {} source "
      "imeages",
      MaxSpritesPerBatch);
  }

  const size_t verticesByteSize = sizeof(vertices[0]) * vertices.size();
  memcpy(mVertexBuffer.mMapping.get(), vertices.data(), verticesByteSize);
  memcpy(mUniformBuffer.mMapping.get(), &batchData, sizeof(batchData));

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
      .renderArea
      = {{0, 0}, {mTargetDimensions.mWidth, mTargetDimensions.mHeight}},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentInfo,
    };
    mVK->CmdBeginRenderingKHR(mCommandBuffer, &renderInfo);
  }

  const VkViewport viewport {
    0,
    0,
    mTargetDimensions.Width<float>(),
    mTargetDimensions.Height<float>(),
    0,
    1};
  mVK->CmdSetViewport(mCommandBuffer, 0, 1, &viewport);
  const VkRect2D scissorRect = {
    {0, 0},
    {mTargetDimensions.mWidth, mTargetDimensions.mHeight},
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
    std::vector<VkDescriptorImageInfo> sourceInfos;
    sourceInfos.reserve(sources.size());
    for (const auto& source: sources) {
      sourceInfos.push_back(VkDescriptorImageInfo {
        .imageView = source,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      });
    }

    VkDescriptorBufferInfo uniformBufferInfo {
      .buffer = mUniformBuffer.mBuffer.get(),
      .range = sizeof(UniformBuffer),
    };

    std::array descriptorWrites {
      VkWriteDescriptorSet {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = mDescriptorSet.mDescriptorSet,
        .dstBinding = 1,
        .descriptorCount = static_cast<uint32_t>(sourceInfos.size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = sourceInfos.data(),
      },
      VkWriteDescriptorSet {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = mDescriptorSet.mDescriptorSet,
        .dstBinding = 2,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &uniformBufferInfo,
      },
    };
    mVK->UpdateDescriptorSets(
      mDevice, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);

    VkDescriptorSet descriptors[] {
      mDescriptorSet.mDescriptorSet,
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
  }// namespace OpenKneeboard::Vulkan

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
    VkClearRect clearRect {
      .rect = {
        { 0, 0,},
        { mTargetDimensions.mWidth, mTargetDimensions.mHeight },
      },
      .layerCount = 1,
    };
    mVK->CmdClearAttachments(mCommandBuffer, 1, &clear, 1, &clearRect);
  }

  mVK->CmdDraw(mCommandBuffer, vertices.size(), 1, 0, 0);

  mVK->CmdEndRenderingKHR(mCommandBuffer);

  mSprites.clear();
  mCommandBuffer = {};
  mTarget = nullptr;
  mClearColor = {};
}

VkVertexInputBindingDescription SpriteBatch::GetVertexBindingDescription() {
  return VkVertexInputBindingDescription {
    .stride = sizeof(Vertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
}

std::array<VkVertexInputAttributeDescription, 4>
SpriteBatch::GetVertexAttributeDescription() {
  return {
    VkVertexInputAttributeDescription {
      .location = 0,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = offsetof(Vertex, mPosition),
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
      .format = VK_FORMAT_R32_UINT,
      .offset = offsetof(Vertex, mTextureIndex),
    },
  };
}

void SpriteBatch::CreateSampler() {
  VkSamplerCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .maxLod = VK_LOD_CLAMP_NONE,
  };
  mSampler = mVK->make_unique<VkSampler>(mDevice, &createInfo, mAllocator);
}

void SpriteBatch::CreateDescriptorSet() {
  auto sampler = mSampler.get();
  VkDescriptorSetLayoutBinding layoutBindings[] {
    VkDescriptorSetLayoutBinding {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = &sampler,
    },
    VkDescriptorSetLayoutBinding {
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .descriptorCount = MaxSpritesPerBatch,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    },
    VkDescriptorSetLayoutBinding {
      .binding = 2,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    },
  };

  VkDescriptorBindingFlags bindingFlags[] {
    0,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
  };

  static_assert(std::size(bindingFlags) == std::size(layoutBindings));

  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsCreateInfo {
    .sType
    = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    .bindingCount = std::size(bindingFlags),
    .pBindingFlags = bindingFlags,
  };

  VkDescriptorSetLayoutCreateInfo layoutCreateInfo {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = &bindingFlagsCreateInfo,
    .flags = 0,
    .bindingCount = std::size(layoutBindings),
    .pBindings = layoutBindings,
  };

  mDescriptorSet.mLayout = mVK->make_unique<VkDescriptorSetLayout>(
    mDevice, &layoutCreateInfo, mAllocator);

  VkDescriptorPoolSize poolSizes[] {
    {
      .type = VK_DESCRIPTOR_TYPE_SAMPLER,
      .descriptorCount = 1,
    },
    {
      .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .descriptorCount = MaxSpritesPerBatch,
    },
  };
  VkDescriptorPoolCreateInfo poolCreateInfo {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = 0,
    .maxSets = 1,
    .poolSizeCount = std::size(poolSizes),
    .pPoolSizes = poolSizes,
  };

  mDescriptorSet.mDescriptorPool
    = mVK->make_unique<VkDescriptorPool>(mDevice, &poolCreateInfo, mAllocator);

  VkDescriptorSetLayout layouts[] {mDescriptorSet.mLayout.get()};
  VkDescriptorSetAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = mDescriptorSet.mDescriptorPool.get(),
    .descriptorSetCount = std::size(layouts),
    .pSetLayouts = layouts,
  };
  check_vkresult(mVK->AllocateDescriptorSets(
    mDevice, &allocInfo, &mDescriptorSet.mDescriptorSet));
}

SpriteBatch::InstanceCreateInfo::InstanceCreateInfo(
  const VkInstanceCreateInfo& base)
  : ExtendedCreateInfo(base, SpriteBatch::REQUIRED_INSTANCE_EXTENSIONS) {
}

template <class T>
concept has_descriptor_indexing_flag
  = requires(T t) { t.descriptorIndexing = true; };

static void EnableDescriptorIndexing(auto* it) {
  if constexpr (has_descriptor_indexing_flag<decltype(*it)>) {
    it->descriptorIndexing = true;
  }
  it->descriptorBindingPartiallyBound = true;
  it->shaderSampledImageArrayNonUniformIndexing = true;
  it->runtimeDescriptorArray = true;
}

SpriteBatch::DeviceCreateInfo::DeviceCreateInfo(const VkDeviceCreateInfo& base)
  : ExtendedCreateInfo(base, SpriteBatch::REQUIRED_DEVICE_EXTENSIONS) {
  bool enabledDescriptorIndexing = false;
  bool enabledDynamicRendering = false;

  for (auto next = reinterpret_cast<const VkBaseOutStructure*>(pNext); next;
       next = next->pNext) {
    auto mut = const_cast<VkBaseOutStructure*>(next);
    switch (next->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES: {
        auto it = reinterpret_cast<VkPhysicalDeviceVulkan12Features*>(mut);
        EnableDescriptorIndexing(it);
        enabledDescriptorIndexing = true;
        continue;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES: {
        auto it = reinterpret_cast<VkPhysicalDeviceVulkan13Features*>(mut);
        it->dynamicRendering = true;
        enabledDynamicRendering = true;
        continue;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR: {
        auto it
          = reinterpret_cast<VkPhysicalDeviceDynamicRenderingFeaturesKHR*>(mut);
        it->dynamicRendering = true;
        enabledDynamicRendering = true;
        continue;
      }
      default:
        continue;
    }
  }

  if (!enabledDescriptorIndexing) {
    auto it = &mDescriptorIndexingFeatures;
    EnableDescriptorIndexing(it);
    it->pNext = const_cast<void*>(pNext);
    pNext = it;
  }

  if (!enabledDynamicRendering) {
    auto it = &mDynamicRenderingFeatures;
    it->dynamicRendering = true;
    it->pNext = const_cast<void*>(pNext);
    pNext = it;
  }
}

}// namespace OpenKneeboard::Vulkan