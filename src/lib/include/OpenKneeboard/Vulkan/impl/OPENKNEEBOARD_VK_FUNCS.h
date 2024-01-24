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
  IT(CmdCopyImage) \
  IT(CmdDrawIndexed) \
  IT(CmdEndRenderingKHR) \
  IT(CmdPipelineBarrier) \
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
  IT(GetImageMemoryRequirements2KHR) \
  IT(GetMemoryWin32HandlePropertiesKHR) \
  IT(GetPhysicalDeviceMemoryProperties) \
  IT(GetPhysicalDeviceProperties2KHR) \
  IT(GetPhysicalDeviceQueueFamilyProperties) \
  IT(GetSemaphoreWin32HandleKHR) \
  IT(ImportSemaphoreWin32HandleKHR) \
  IT(MapMemory) \
  IT(QueueSubmit) \
  IT(QueueWaitIdle) \
  IT(ResetCommandBuffer) \
  IT(ResetFences) \
  IT(UnmapMemory) \
  IT(UpdateDescriptorSets) \
  IT(WaitForFences)