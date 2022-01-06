#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace OpenKneeboard::SHM::Flags {
constexpr uint64_t HEADLOCKED = 1;
constexpr uint64_t DISCARD_DEPTH_INFORMATION = 1 << 1;
constexpr uint64_t FEEDER_ATTACHED = 1 << 2;
constexpr uint64_t LOCKED = 1 << 3;
};// namespace OpenKneeboard::Flags

namespace OpenKneeboard::SHM {

#pragma pack(push)
struct Header {
  static const uint32_t VERSION = 1;

  uint64_t Flags;
  float x, floorY, eyeY, z;
  float rx, ry, rz;
  // Meters
  float VirtualWidth, VirtualHeight;
  float ZoomScale;
  // Pixels
  uint16_t ImageWidth, ImageHeight;

  uint64_t SequenceNumber = 0;
};
struct Pixel {
  uint8_t r;
  uint8_t g;
  uint8_t b;
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
