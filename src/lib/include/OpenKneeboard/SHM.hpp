// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "VRSettings.hpp"

#include <OpenKneeboard/Pixels.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <boost/container/static_vector.hpp>
#include <felly/non_copyable.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

namespace OpenKneeboard::SHM {

static constexpr auto SwapChainLength = Config::SHMSwapchainLength;

// Distances in metres, positions in radians.
struct VRLayer {
  VRPose mPose;
  Geometry2D::Size<float> mPhysicalSize;

  bool mEnableGazeZoom {true};
  float mZoomScale = 2.0f;
  GazeTargetScale mGazeTargetScale {};
  VROpacitySettings mOpacity {};
  PixelRect mLocationOnTexture {};
};

static constexpr DXGI_FORMAT SHARED_TEXTURE_PIXEL_FORMAT
  = DXGI_FORMAT_B8G8R8A8_UNORM;
static constexpr bool SHARED_TEXTURE_IS_PREMULTIPLIED = true;

struct LayerSprite {
  PixelRect mSourceRect;
  PixelRect mDestRect;
  float mOpacity;
};

// This needs to be kept in sync with `SHM::ActiveConsumers`
enum class ConsumerKind : uint32_t {
  OpenVR = 1 << 0,
  OpenXR_D3D11 = 1 << 1,
  OpenXR_D3D12 = 1 << 2,
  OpenXR_Vulkan2 = 1 << 3,// for XR_KHR_vulkan_enable2
  Viewer = std::numeric_limits<uint32_t>::max(),
};

struct Config final {
  uint64_t mGlobalInputLayerID {};
  VRRenderSettings mVR {};
  PixelSize mTextureSize {};
  std::array<float, 4> mTint {1, 1, 1, 1};
};
static_assert(std::is_standard_layout_v<Config>);
struct LayerConfig final {
  uint64_t mLayerID {};

  bool mVREnabled {false};
  SHM::VRLayer mVR {};
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

  [[nodiscard]]
  uint64_t GetFrameCountForMetricsOnly() const;

  NextFrameInfo BeginFrame() noexcept;
  void SubmitFrame(
    const NextFrameInfo&,
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

struct Frame : felly::non_copyable {
  enum class Error {
    IncorrectGPU,
    NoFeeder,
    UnusableHandles,
  };

  Config mConfig {};
  boost::container::static_vector<LayerConfig, MaxViewCount> mLayers {};

  HANDLE mTexture {};

  HANDLE mFence {};
  int64_t mFenceIn {};

  uint8_t mIndex {};
};

enum class ReaderState;

class Reader {
 public:
  using State = ReaderState;
  Reader() = delete;
  Reader(ConsumerKind, uint64_t gpuLUID);
  virtual ~Reader();

  operator bool() const;
  uint64_t GetFrameCountForMetricsOnly() const;

  std::expected<Frame, Frame::Error> MaybeGet();

 protected:
  virtual void OnSessionChanged() = 0;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard::SHM
