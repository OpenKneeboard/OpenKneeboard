#pragma once

#include "bitflags.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

namespace OpenKneeboard::SHM {

#pragma pack(push)
struct VRConfig {
  static constexpr uint16_t VERSION = 1;

  enum class Flags : uint32_t {
    HEADLOCKED = 1 << 0,
    DISCARD_DEPTH_INFORMATION = 1 << 1,
  };

  // Distances in meters, rotations in radians
  float x = 0.15f, floorY = 0.6f, eyeY = -0.7f, z = -0.4f;
  float rx = -2 * std::numbers::pi_v<float> / 5,
        ry = -std::numbers::pi_v<float> / 32, rz = 0.0f;
  float height = 0.25f;
  float zoomScale = 2.0f;

  Flags flags;
};

struct FlatConfig {
  static constexpr uint16_t VERSION = 1;

  enum HorizontalAlignment {
    HALIGN_LEFT,
    HALIGN_CENTER,
    HALIGN_RIGHT,
  };
  enum VerticalAlignment {
    VALIGN_TOP,
    VALIGN_MIDDLE,
    VALIGN_BOTTOM,
  };

  uint8_t heightPercent = 60;
  uint32_t paddingPixels = 10;
  // In case it covers up menus etc
  float opacity = 0.8f;

  HorizontalAlignment horizontalAlignment = HALIGN_RIGHT;
  VerticalAlignment verticalAlignment = VALIGN_MIDDLE;
};

struct Config final {
  static constexpr uint16_t VERSION = 1;

  uint16_t imageWidth, imageHeight; // Pixels
  VRConfig vr;
  FlatConfig flat;
};

/// 32-bit BGRA with pre-multiplied alpha
struct Pixel final {
  static constexpr bool IS_PREMULTIPLIED_B8G8R8A8 = true;
  uint8_t b;
  uint8_t g;
  uint8_t r;
  uint8_t a;
};
static_assert(sizeof(Pixel) == 4);
static_assert(offsetof(Pixel, b) == 0);
static_assert(offsetof(Pixel, a) == 3);
#pragma pack(pop)

class Impl;

class Writer final {
 public:
  Writer();
  ~Writer();

  operator bool() const;
  void Update(const Config& config, const std::vector<Pixel>& pixels);

 private:
  std::shared_ptr<Impl> p;
};

class Snapshot final {
 private:
  std::shared_ptr<std::vector<std::byte>> mBytes;

 public:
  Snapshot();
  Snapshot(std::vector<std::byte>&& bytes);

  uint32_t GetSequenceNumber() const;
  const Config* const GetConfig() const;
  const Pixel* const GetPixels() const;

  operator bool() const;
};

class Reader final {
 public:
  Reader();
  ~Reader();

  operator bool() const;
  Snapshot MaybeGet() const;

 private:
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard::SHM

namespace OpenKneeboard {
  template<>
  constexpr bool is_bitflags_v<SHM::VRConfig::Flags> = true;
}
