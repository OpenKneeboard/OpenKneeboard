#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace YAVRK {
constexpr uint16_t IPC_VERSION = 1;

namespace Flags {
constexpr uint64_t HEADLOCKED = 1;
constexpr uint64_t DISCARD_DEPTH_INFORMATION = 1 << 1;
constexpr uint64_t FEEDER_DETACHED = 1 << 2;
};// namespace Flags

#pragma pack(push)
struct SHMHeader {
  uint16_t Version = 0;
  uint64_t Flags;
  float x, y, z;
  float rx, ry, rz;
  // Meters
  float VirtualWidth, VirtualHeight;
  // Pixels
  uint16_t ImageWidth, ImageHeight;
};
#pragma pack(pop)

class SHM final {
 public:
#pragma pack(push)
  struct Pixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  };
#pragma pack(pop)
  ~SHM();
  SHMHeader Header() const;
  Pixel* ImageData() const;
  uint32_t ImageDataSize() const;
  operator bool() const;

  static SHM GetOrCreate(const SHMHeader& header);
  static SHM MaybeGet();

  SHM() = default;

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  SHM(const std::shared_ptr<Impl>&);
};
}// namespace YAVRK
