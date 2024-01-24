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

#include "viewer-vulkan.h"

namespace OpenKneeboard::Viewer {

VulkanRenderer::VulkanRenderer(uint64_t luid) {
  // TODO
}

VulkanRenderer::~VulkanRenderer() = default;

SHM::CachedReader* VulkanRenderer::GetSHM() {
  return &mSHM;
}

std::wstring_view VulkanRenderer::GetName() const noexcept {
  return L"Vulkan";
}

void VulkanRenderer::Initialize(uint8_t swapchainLength) {
  // TODO
}

void VulkanRenderer::SaveTextureToFile(
  SHM::IPCClientTexture*,
  const std::filesystem::path&) {
  // TODO
}

uint64_t VulkanRenderer::Render(
  SHM::IPCClientTexture* sourceTexture,
  const PixelRect& sourceRect,
  HANDLE destTexture,
  const PixelSize& destTextureDimensions,
  const PixelRect& destRect,
  HANDLE fence,
  uint64_t fenceValueIn) {
  return fenceValueIn;
}

}// namespace OpenKneeboard::Viewer