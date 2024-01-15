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

#include "FlatConfig.h"
#include "VRConfig.h"

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include <d3d11.h>
#include <d3d11_3.h>

namespace OpenKneeboard::SHM {

using VRPosition = VRAbsolutePosition;

struct FrameMetadata;

static constexpr DXGI_FORMAT SHARED_TEXTURE_PIXEL_FORMAT
  = DXGI_FORMAT_B8G8R8A8_UNORM;
static constexpr bool SHARED_TEXTURE_IS_PREMULTIPLIED = true;

class LayerTextureCache {
 public:
  LayerTextureCache() = delete;
  LayerTextureCache(const winrt::com_ptr<ID3D11Texture2D>&);
  virtual ~LayerTextureCache();

  ID3D11Texture2D* GetD3D11Texture();

 private:
  winrt::com_ptr<ID3D11Texture2D> mD3D11Texture;
};

using LayersTextureCache
  = std::array<std::shared_ptr<LayerTextureCache>, MaxLayers>;

std::wstring SharedTextureName(
  uint64_t sessionID,
  uint8_t layerIndex,
  uint32_t sequenceNumber);

constexpr UINT DEFAULT_D3D11_BIND_FLAGS
  = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
constexpr UINT DEFAULT_D3D11_MISC_FLAGS = 0;

winrt::com_ptr<ID3D11Texture2D> CreateCompatibleTexture(
  ID3D11Device*,
  UINT bindFlags = DEFAULT_D3D11_BIND_FLAGS,
  UINT miscFlags = DEFAULT_D3D11_MISC_FLAGS,
  DXGI_FORMAT format = SHARED_TEXTURE_PIXEL_FORMAT);

// This needs to be kept in sync with `SHM::ActiveConsumers`
enum class ConsumerKind : uint32_t {
  SteamVR = 1 << 0,
  OpenXR = 1 << 1,
  OculusD3D11 = 1 << 2,
  OculusD3D12 = 1 << 3,
  NonVRD3D11 = 1 << 4,
  Viewer = ~(0ui32),
};

class ConsumerPattern final {
 public:
  ConsumerPattern();
  ConsumerPattern(std::underlying_type_t<ConsumerKind>(consumerKindMask));
  ConsumerPattern(ConsumerKind kind)
    : mKindMask(std::underlying_type_t<ConsumerKind>(kind)) {
  }

  bool Matches(ConsumerKind) const;

  std::underlying_type_t<ConsumerKind> GetRawMaskForDebugging() const;

 private:
  std::underlying_type_t<ConsumerKind> mKindMask {0};
};

struct Config final {
  uint64_t mGlobalInputLayerID {};
  VRRenderConfig mVR {};
  NonVRConstrainedPosition mFlat {};
  ConsumerPattern mTarget {};
};
static_assert(std::is_standard_layout_v<Config>);
struct LayerConfig final {
  uint64_t mLayerID;
  uint16_t mImageWidth, mImageHeight;// Pixels
  VRAbsolutePosition mVR;

  bool IsValid() const;
};
static_assert(std::is_standard_layout_v<LayerConfig>);

class Impl;

class Writer final {
 public:
  Writer();
  ~Writer();

  void Detach();

  operator bool() const;

  void Update(
    const Config& config,
    const std::vector<LayerConfig>& layers,
    HANDLE fence);

  UINT GetNextTextureIndex() const;

  uint64_t GetSessionID() const;
  uint32_t GetNextSequenceNumber() const;

  // "Lockable" C++ named concept: supports std::unique_lock
  void lock();
  bool try_lock();
  void unlock();

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

struct IPCTextureReadResources;
struct IPCLayerTextureReadResources;

class Snapshot final {
 public:
  enum class State {
    Empty,
    IncorrectKind,
    Valid,
  };
  // marker for constructor
  struct incorrect_kind_t {};
  static constexpr const incorrect_kind_t incorrect_kind {};

  Snapshot(nullptr_t);
  Snapshot(incorrect_kind_t);

  Snapshot(
    const FrameMetadata& header,
    ID3D11DeviceContext4*,
    ID3D11Fence*,
    const LayersTextureCache&,
    IPCTextureReadResources*);
  ~Snapshot();

  /// Changes even if the feeder restarts with frame ID 0
  size_t GetRenderCacheKey() const;
  Config GetConfig() const;
  uint8_t GetLayerCount() const;
  const LayerConfig* GetLayerConfig(uint8_t layerIndex) const;

  template <std::derived_from<LayerTextureCache> T>
  T* GetLayerGPUResources(uint8_t layerIndex) const {
    if (layerIndex >= this->GetLayerCount()) [[unlikely]] {
      dprintf(
        "Asked for layer {}, but there are {} layers",
        layerIndex,
        this->GetLayerCount());
      OPENKNEEBOARD_BREAK;
      return nullptr;
    }
    const auto ret
      = std::dynamic_pointer_cast<T>(mLayerTextures.at(layerIndex));
    if (!ret) [[unlikely]] {
      dprint("Layer texture cache type mismatch");
      OPENKNEEBOARD_BREAK;
      return nullptr;
    }
    return ret.get();
  }

  bool IsValid() const;
  State GetState() const;

  // Use GetRenderCacheKey() instead for almost all purposes
  uint64_t GetSequenceNumberForDebuggingOnly() const;
  Snapshot() = delete;

 private:
  std::shared_ptr<FrameMetadata> mHeader;
  LayersTextureCache mLayerTextures;

  State mState;
};

class Reader {
 public:
  Reader();
  ~Reader();

  operator bool() const;
  /// Do not use for caching - use GetRenderCacheKey instead
  uint32_t GetFrameCountForMetricsOnly() const;

  /** Fetch the render cache key, and mark the consumer kind as active if
   * enabled.
   *
   *
   * Changes even if the feeder restarts from frame ID 0.
   */
  size_t GetRenderCacheKey(ConsumerKind kind);

  uint64_t GetSessionID() const;

  Snapshot MaybeGetUncached(
    ID3D11DeviceContext4*,
    ID3D11Fence*,
    const LayersTextureCache&,
    ConsumerKind) const;

 protected:
  class Impl;
  std::shared_ptr<Impl> p;
};

class CachedReader : public Reader {
 public:
  virtual ~CachedReader();
  Snapshot MaybeGet(ID3D11Device* device, ConsumerKind kind);

 protected:
  virtual std::shared_ptr<LayerTextureCache> CreateLayerTextureCache(
    const winrt::com_ptr<ID3D11Texture2D>&);

 private:
  void InitDXResources(ID3D11Device*);

  ID3D11Device* mDevice {nullptr};
  uint64_t mSessionID = 0;
  winrt::com_ptr<ID3D11DeviceContext4> mContext;
  winrt::com_ptr<ID3D11Fence> mFence;
  winrt::handle mFenceHandle;
  SHM::LayersTextureCache mTextures;

  Snapshot mCache {nullptr};
  ConsumerKind mCachedConsumerKind;
  uint64_t mCachedSequenceNumber {};

  // Fetch a (possibly-cached) snapshot
  Snapshot MaybeGet(
    ID3D11DeviceContext4*,
    ID3D11Fence*,
    const LayersTextureCache&,
    ConsumerKind) noexcept;
};

}// namespace OpenKneeboard::SHM
