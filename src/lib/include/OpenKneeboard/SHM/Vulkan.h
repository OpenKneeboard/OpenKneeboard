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
#include <OpenKneeboard/SHM.h>

namespace OpenKneeboard::SHM::Vulkan {

class Texture final : public SHM::IPCClientTexture {};

class CachedReader : public SHM::CachedReader, protected SHM::IPCTextureCopier {
 public:
  CachedReader() = delete;
  CachedReader(ConsumerKind);
  virtual ~CachedReader();

  // void InitializeCache(uint8_t swapchainLength);

 protected:
  virtual void Copy(
    uint8_t swapchainIndex,
    HANDLE sourceTexture,
    IPCClientTexture* destinationTexture,
    HANDLE fence,
    uint64_t fenceValueIn,
    uint64_t fenceValueOut) noexcept override;

  virtual std::shared_ptr<SHM::IPCClientTexture> CreateIPCClientTexture(
    uint8_t swapchainIndex) noexcept override;
};

};// namespace OpenKneeboard::SHM::Vulkan