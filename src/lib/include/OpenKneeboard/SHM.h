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

#include <Windows.h>
#include <d3d11.h>
#include <shims/winrt.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "FlatConfig.h"
#include "VRConfig.h"

namespace OpenKneeboard::SHM {

struct Header;

static constexpr DXGI_FORMAT SHARED_TEXTURE_PIXEL_FORMAT
  = DXGI_FORMAT_B8G8R8A8_UNORM;
static constexpr bool SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8 = true;
static constexpr bool SHARED_TEXTURE_IS_PREMULTIPLIED = true;

std::wstring SharedTextureName(
  uint64_t sessionID,
  uint8_t layerIndex,
  uint32_t sequenceNumber);

#pragma pack(push)
struct Config final {
  static constexpr uint16_t VERSION = 3;
  uint64_t mGlobalInputLayerID;
  VRRenderConfig mVR;
  FlatConfig mFlat;
};
struct LayerConfig final {
  static constexpr uint16_t VERSION = 2;
  uint64_t mLayerID;
  uint16_t mImageWidth, mImageHeight;// Pixels
  VRLayerConfig mVR;

  bool IsValid() const;
};
#pragma pack(pop)

class Impl;

class Writer final {
 public:
  Writer();
  ~Writer();

  operator bool() const;
  void Update(const Config& config, const std::vector<LayerConfig>& layers);

  UINT GetPreviousTextureKey() const;
  UINT GetNextTextureKey() const;
  UINT GetNextTextureIndex() const;

  uint64_t GetSessionID() const;
  uint32_t GetNextSequenceNumber() const;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

struct TextureReadResources;
struct LayerTextureReadResources;

class SharedTexture final {
 public:
  SharedTexture();
  SharedTexture(SharedTexture&&);
  SharedTexture(
    const Header& header,
    ID3D11Device* d3d,
    uint8_t layerIndex,
    TextureReadResources* r);
  ~SharedTexture();

  bool IsValid() const;

  ID3D11Texture2D* GetTexture() const;
  IDXGISurface* GetSurface() const;
  ID3D11ShaderResourceView* GetShaderResourceView() const;

  SharedTexture(const SharedTexture&) = delete;

 private:
  UINT mKey = 0;
  LayerTextureReadResources* mResources = nullptr;
};

class Snapshot final {
 private:
  std::unique_ptr<Header> mHeader;
  TextureReadResources* mResources;

 public:
  Snapshot();
  Snapshot(const Header& header, TextureReadResources*);
  ~Snapshot();

  /// Changes even if the feeder restarts with frame ID 0
  size_t GetRenderCacheKey() const;
  Config GetConfig() const;
  uint8_t GetLayerCount() const;
  const LayerConfig* GetLayerConfig(uint8_t layerIndex) const;
  SharedTexture GetLayerTexture(ID3D11Device*, uint8_t layerIndex) const;

  bool IsValid() const;
};

class Reader final {
 public:
  Reader();
  ~Reader();

  operator bool() const;
  Snapshot MaybeGet() const;
  /// Changes even if the feeder restarts with frame ID 0
  size_t GetRenderCacheKey() const;
  /// Do not use for caching - use GetRenderCacheKey instead
  uint32_t GetFrameCountForMetricsOnly() const;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard::SHM
