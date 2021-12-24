#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace YAVRK {
static const uint32_t IPC_VERSION = 1;

struct SHMHeader {
  uint32_t Version = 0;
  uint64_t Flags;
  float x, y, z;
  float rx, ry, rz;
  uint16_t Width, Height;
};

class SHM final {
 public:
  ~SHM();
  SHMHeader Header() const;
  std::byte* ImageData() const;
  operator bool() const;

  static SHM GetOrCreate(const SHMHeader& header);
  static SHM MaybeGet();
 private:
  class Impl;
  std::shared_ptr<Impl> p;

  SHM(std::shared_ptr<Impl>);
};
}// namespace YAVRK