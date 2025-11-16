// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "viewer-settings.hpp"

#include <OpenKneeboard/SHM.hpp>

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