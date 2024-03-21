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

#include "viewer-settings.h"

#include <OpenKneeboard/SHM.h>

#include <filesystem>
#include <string>

namespace OpenKneeboard::Viewer {

class Renderer {
 public:
  virtual ~Renderer();
  virtual SHM::CachedReader* GetSHM() = 0;

  virtual std::wstring_view GetName() const noexcept = 0;

  virtual void Initialize(uint8_t swapchainLength) = 0;

  virtual void SaveTextureToFile(
    SHM::IPCClientTexture*,
    const std::filesystem::path&)
    = 0;

  /** Render the texture.
   *
   * Note HANDLE is an NT handle, not a classic Windows handle; these
   * need to be handled differently by most graphics APIs.
   *
   * @return a fence value to wait on
   */
  virtual uint64_t Render(
    SHM::IPCClientTexture* sourceTexture,
    const PixelRect& sourceRect,
    HANDLE destTexture,
    const PixelSize& destTextureDimensions,
    const PixelRect& destRect,
    HANDLE fence,
    uint64_t fenceValueIn)
    = 0;
};

}// namespace OpenKneeboard::Viewer