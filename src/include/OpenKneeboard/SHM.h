#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace OpenKneeboard::Flags {
constexpr uint64_t HEADLOCKED = 1;
constexpr uint64_t DISCARD_DEPTH_INFORMATION = 1 << 1;
constexpr uint64_t FEEDER_ATTACHED= 1 << 2;
};// namespace OpenKneeboard::Flags

namespace OpenKneeboard::SHM {

#pragma pack(push)
struct Header {
  static const uint32_t VERSION = 1;

  uint64_t Flags;
  float x, y, z;
  float rx, ry, rz;
  // Meters
  float VirtualWidth, VirtualHeight;
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

class Reader final {
 public:
  Reader();
  ~Reader();

  operator bool() const;
  std::optional<std::tuple<Header, std::vector<Pixel>>> MaybeGet() const;

 private:
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard::SHM
