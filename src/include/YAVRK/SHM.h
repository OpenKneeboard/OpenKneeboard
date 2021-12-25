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
};// namespace Flags

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

class SHM final {
 public:
  ~SHM();
  SHMHeader Header() const;
  /// R8G8B8A8
  std::byte* ImageData() const;
  uint32_t ImageDataSize() const;
  operator bool() const;

  static SHM GetOrCreate(const SHMHeader& header);
  static SHM MaybeGet();

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  SHM(std::shared_ptr<Impl>);
};
}// namespace YAVRK