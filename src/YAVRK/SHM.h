#pragma once

#include <cstddef>
#include <cstdint>

namespace YAVRK {
  struct SHMHeader {
    static const uint32_t VERSION = 1;
    uint32_t Version = VERSION;
    uint64_t Flags;
    float x, y, z;
    float rx, ry, rz;
    uint16_t Width, Height;
  };
  struct SHM {
    SHMHeader Header;
    std::byte* R8G8B8A8Data;
  };
};