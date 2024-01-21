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
  const VkAllocationCallbacks* allocator) {
  OPENKNEEBOARD_TraceLoggingScope("SpriteBatch::SpriteBatch()");

  namespace Shaders = Shaders::SPIRV::SpriteBatch;

  VkShaderModuleCreateInfo psCreateInfo {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = Shaders::PS.size(),
    .pCode = reinterpret_cast<const uint32_t*>(Shaders::PS.data()),
  };

  VkShaderModuleCreateInfo vsCreateInfo {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = Shaders::VS.size(),
    .pCode = reinterpret_cast<const uint32_t*>(Shaders::VS.data()),
  };

  mPixelShader
    = dispatch->make_unique<VkShaderModule>(device, &psCreateInfo, allocator);
  mVertexShader
    = dispatch->make_unique<VkShaderModule>(device, &vsCreateInfo, allocator);
}

SpriteBatch::~SpriteBatch() {
  OPENKNEEBOARD_TraceLoggingWrite("SpriteBatch::~SpriteBatch()");
}

}// namespace OpenKneeboard::Vulkan