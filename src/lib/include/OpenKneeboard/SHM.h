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

#include <OpenKneeboard/Pixels.h>

#include <OpenKneeboard/dprint.h>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

namespace OpenKneeboard::SHM {

namespace Detail {
struct FrameMetadata;

struct DeviceResources;
struct IPCHandles;

}// namespace Detail

// Distances in metres, positions in radians.
struct VRLayer {
  VRPose mPose;
  Geometry2D::Size<float> mPhysicalSize;

  bool mEnableGazeZoom {true};
  float mZoomScale = 2.0f;
  GazeTargetScale mGazeTargetScale {};
  VROpacityConfig mOpacity {};
  PixelRect mLocationOnTexture {};
};

struct NonVRLayer {
  NonVRConstrainedPosition mPosition;
  PixelRect mLocationOnTexture;
  float mOpacity;
};

static constexpr DXGI_FORMAT SHARED_TEXTURE_PIXEL_FORMAT
  = DXGI_FORMAT_B8G8R8A8_UNORM;
static constexpr bool SHARED_TEXTURE_IS_PREMULTIPLIED = true;

struct LayerSprite {
  PixelRect mSourceRect;
  PixelRect mDestRect;
  float mOpacity;
};

// See SHM::D3D11::IPCClientTexture etc
class IPCClientTexture {
 public:
  IPCClientTexture() = delete;
  virtual ~IPCClientTexture();

  inline const PixelSize GetDimensions() const {
    return mDimensions;
  }

  inline const uint8_t GetSwapchainIndex() const {
    return mSwapchainIndex;
  }

 protected:
  IPCClientTexture(const PixelSize&, uint8_t swapchainIndex);

 private:
  const PixelSize mDimensions;
  const uint8_t mSwapchainIndex;
};

// See SHM::D3D11::CachedReader etc
class IPCTextureCopier {
 public:
  virtual ~IPCTextureCopier();
  virtual void Copy(
    HANDLE sourceTexture,
    IPCClientTexture* destinationTexture,
    HANDLE fence,
    uint64_t fenceValueIn,
    uint64_t fenceValueOut) noexcept
    = 0;
};

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
  ConsumerPattern mTarget {};
  PixelSize mTextureSize {};
  std::array<float, 4> mTint {1, 1, 1, 1};
};
static_assert(std::is_standard_layout_v<Config>);
struct LayerConfig final {
  uint64_t mLayerID {};

  bool mVREnabled {false};
  SHM::VRLayer mVR {};
  bool mNonVREnabled {false};
  SHM::NonVRLayer mNonVR {};
};
static_assert(std::is_standard_layout_v<LayerConfig>);

enum class WriterState;

class Writer final {
 public:
  struct NextFrameInfo {
    uint8_t mTextureIndex {};
    LONG64 mFenceOut {};
  };

  Writer() = delete;
  Writer(uint64_t gpuLuid);
  ~Writer();

  void Detach();

  operator bool() const;

  void SubmitEmptyFrame();

  NextFrameInfo BeginFrame() noexcept;
  void SubmitFrame(
    const Config& config,
    const std::vector<LayerConfig>& layers,
    HANDLE texture,
    HANDLE fence);

  // "Lockable" C++ named concept: supports std::unique_lock
  void lock();
  bool try_lock();
  void unlock();

 private:
  using State = WriterState;
  class Impl;
  std::shared_ptr<Impl> p;
};

class Snapshot final {
 public:
  enum class State {
    Empty,
    IncorrectKind,
    IncorrectGPU,
    ValidWithoutTexture,
    ValidWithTexture,
  };
  // marker for constructor
  struct incorrect_kind_t {};
  static constexpr const incorrect_kind_t incorrect_kind {};
  struct incorrect_gpu_t {};
  static constexpr const incorrect_gpu_t incorrect_gpu {};

  Snapshot(nullptr_t);
  Snapshot(incorrect_kind_t);
  Snapshot(incorrect_gpu_t);

  Snapshot(
    Detail::FrameMetadata*,
    IPCTextureCopier* copier,
    Detail::IPCHandles* source,
    const std::shared_ptr<IPCClientTexture>& dest);
  Snapshot(Detail::FrameMetadata*);
  ~Snapshot();

  uint64_t GetSessionID() const;
  /// Changes even if the feeder restarts with frame ID 0
  size_t GetRenderCacheKey() const;
  Config GetConfig() const;
  uint8_t GetLayerCount() const;
  const LayerConfig* GetLayerConfig(uint8_t layerIndex) const;

  template <std::derived_from<IPCClientTexture> T>
  T* GetTexture() const {
    if (mState != State::ValidWithTexture) [[unlikely]] {
      OPENKNEEBOARD_LOG_AND_FATAL(
        "Called SHM::Snapshot::GetTexture() with invalid state {}",
        static_cast<uint8_t>(mState));
    }
    const auto ret = std::dynamic_pointer_cast<T>(mIPCTexture);
    if (!ret) [[unlikely]] {
      OPENKNEEBOARD_LOG_AND_FATAL("Layer texture cache type mismatch");
    }
    return ret.get();
  }

  State GetState() const;

  constexpr bool HasMetadata() const {
    return (mState == State::ValidWithoutTexture)
      || (mState == State::ValidWithTexture);
  }

  constexpr bool HasTexture() const {
    return mState == State::ValidWithTexture;
  }

  // Use GetRenderCacheKey() instead for almost all purposes
  uint64_t GetSequenceNumberForDebuggingOnly() const;
  Snapshot() = delete;

 private:
  std::shared_ptr<Detail::FrameMetadata> mHeader;
  std::shared_ptr<IPCClientTexture> mIPCTexture;

  State mState;
};

enum class ReaderState;

class Reader {
 public:
  using State = ReaderState;
  Reader();
  ~Reader();

  operator bool() const;
  /// Do not use for caching - use GetRenderCacheKey instead
  uint64_t GetFrameCountForMetricsOnly() const;

  /** Fetch the render cache key, and mark the consumer kind as active if
   * enabled.
   *
   *
   * Changes even if the feeder restarts from frame ID 0.
   */
  size_t GetRenderCacheKey(ConsumerKind kind) const;

  uint64_t GetSessionID() const;

 protected:
  Snapshot MaybeGetUncached(ConsumerKind);
  Snapshot MaybeGetUncached(
    uint64_t gpuLUID,
    IPCTextureCopier* copier,
    const std::shared_ptr<IPCClientTexture>& dest,
    ConsumerKind) const;

  class Impl;
  std::shared_ptr<Impl> p;
};

class CachedReader : public Reader {
 public:
  CachedReader() = delete;
  CachedReader(IPCTextureCopier*, ConsumerKind);
  virtual ~CachedReader();

  Snapshot MaybeGetMetadata();
  virtual Snapshot MaybeGet(
    const std::source_location& loc = std::source_location::current());

 protected:
  void InitializeCache(uint64_t gpuLUID, uint8_t swapchainLength);

  virtual std::shared_ptr<IPCClientTexture>
  CreateIPCClientTexture(const PixelSize&, uint8_t swapchainIndex) noexcept = 0;

  virtual void ReleaseIPCHandles() = 0;

 private:
  IPCTextureCopier* mTextureCopier {nullptr};
  ConsumerKind mConsumerKind {};

  uint64_t mGPULUID {};
  uint64_t mCacheKey {~(0ui64)};
  uint64_t mSessionID {};
  std::deque<Snapshot> mCache;
  uint8_t mSwapchainIndex {};

  std::vector<std::shared_ptr<IPCClientTexture>> mClientTextures;

  std::shared_ptr<IPCClientTexture> GetIPCClientTexture(
    const PixelSize&,
    uint8_t swapchainIndex) noexcept;

  void UpdateSession();
};

}// namespace OpenKneeboard::SHM
