// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#define OPENKNEEBOARD_VK_FUNCS \
  IT(AllocateCommandBuffers) \
  IT(AllocateDescriptorSets) \
  IT(AllocateMemory) \
  IT(BeginCommandBuffer) \
  IT(BindBufferMemory) \
  IT(BindImageMemory2KHR) \
  IT(CmdBeginRenderingKHR) \
  IT(CmdBindDescriptorSets) \
  IT(CmdBindVertexBuffers) \
  IT(CmdBindPipeline) \
  IT(CmdBlitImage) \
  IT(CmdClearAttachments) \
  IT(CmdClearColorImage) \
  IT(CmdCopyImage) \
  IT(CmdDraw) \
  IT(CmdEndRenderingKHR) \
  IT(CmdPipelineBarrier) \
  IT(CmdPushConstants) \
  IT(CmdSetScissor) \
  IT(CmdSetViewport) \
  IT(CreateBuffer) \
  IT(CreateCommandPool) \
  IT(CreateDebugUtilsMessengerEXT) \
  IT(CreateDescriptorPool) \
  IT(CreateDescriptorSetLayout) \
  IT(CreateDevice) \
  IT(CreateFence) \
  IT(CreateGraphicsPipelines) \
  IT(CreateImage) \
  IT(CreateImageView) \
  IT(CreateInstance) \
  IT(CreatePipelineLayout) \
  IT(CreateSampler) \
  IT(CreateSemaphore) \
  IT(CreateShaderModule) \
  IT(DestroyBuffer) \
  IT(DestroyCommandPool) \
  IT(DestroyDebugUtilsMessengerEXT) \
  IT(DestroyDescriptorPool) \
  IT(DestroyDescriptorSetLayout) \
  IT(DestroyDevice) \
  IT(DestroyFence) \
  IT(DestroyImage) \
  IT(DestroyImageView) \
  IT(DestroyPipeline) \
  IT(DestroyPipelineLayout) \
  IT(DestroySampler) \
  IT(DestroySemaphore) \
  IT(DestroyShaderModule) \
  IT(EndCommandBuffer) \
  IT(EnumeratePhysicalDevices) \
  IT(FreeCommandBuffers) \
  IT(FreeMemory) \
  IT(FlushMappedMemoryRanges) \
  IT(GetBufferMemoryRequirements) \
  IT(GetDescriptorSetLayoutBindingOffsetEXT) \
  IT(GetDescriptorSetLayoutSizeEXT) \
  IT(GetDeviceQueue) \
  IT(GetImageMemoryRequirements) \
  IT(GetImageMemoryRequirements2KHR) \
  IT(GetImageSubresourceLayout) \
  IT(GetMemoryWin32HandlePropertiesKHR) \
  IT(GetPhysicalDeviceMemoryProperties) \
  IT(GetPhysicalDeviceProperties2KHR) \
  IT(GetPhysicalDeviceQueueFamilyProperties) \
  IT(GetSemaphoreWin32HandleKHR) \
  IT(ImportSemaphoreWin32HandleKHR) \
  IT(InvalidateMappedMemoryRanges) \
  IT(MapMemory) \
  IT(QueueSubmit) \
  IT(QueueWaitIdle) \
  IT(ResetCommandBuffer) \
  IT(ResetFences) \
  IT(UnmapMemory) \
  IT(UpdateDescriptorSets) \
  IT(WaitForFences) \
  IT(WaitSemaphoresKHR)
