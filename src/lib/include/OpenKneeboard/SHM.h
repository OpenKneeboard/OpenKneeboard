#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

namespace OpenKneeboard::SHM::Flags {
constexpr uint64_t HEADLOCKED = 1;
constexpr uint64_t DISCARD_DEPTH_INFORMATION = 1 << 1;
constexpr uint64_t FEEDER_ATTACHED = 1 << 2;
constexpr uint64_t LOCKED = 1 << 3;
};// namespace OpenKneeboard::SHM::Flags

namespace OpenKneeboard::SHM {

#pragma pack(push)
struct VRConfig {
  // Distances in meters, rotations in radians
  float x = 0.15f, floorY = 0.6f, eyeY = -0.7f, z = -0.4f;
  float rx = -2 * std::numbers::pi_v<float> / 5,
        ry = -std::numbers::pi_v<float> / 32, rz = 0.0f;
  float height = 0.25f;
  float zoomScale = 2.0f;
};

struct FlatConfig {
  enum HorizontalAlignment{
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

struct Header {
  static const uint32_t VERSION = 1;
  uint64_t sequenceNumber = 0;
  uint64_t flags = 0;
  uint16_t imageWidth, imageHeight;// Pixels

  VRConfig vr;
  FlatConfig flat;
};

struct Pixel {
  uint8_t b;
  uint8_t g;
  uint8_t r;
  uint8_t a;
};
#pragma pack(pop)

class Impl;

class Writer final {
 public:
  Writer();
  ~Writer();

  operator bool() const;
  void Update(const Header& header, const std::vector<Pixel>& pixels);

 private:
  std::shared_ptr<Impl> p;
};

class Snapshot final {
 private:
  std::shared_ptr<std::vector<std::byte>> mBytes;

 public:
  Snapshot();
  Snapshot(std::vector<std::byte>&& bytes);

  const Header* const GetHeader() const;
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
