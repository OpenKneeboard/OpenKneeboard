// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <OpenKneeboard/bitflags.hpp>

#include <Windows.h>

namespace OpenKneeboard::DDS {

static constexpr char Magic[] {'D', 'D', 'S', ' '};

// Copied from MSDN, DDS_PIXELFORMAT
struct PixelFormat {
  enum class Flags : DWORD {
    AlphaPixels = 1,
    Alpha = 2,
    FourCC = 0x4,
    RGB = 0x40,
    YUV = 0x200,
    Luminace = 0x20000,
  };
  DWORD dwSize {sizeof(PixelFormat)};
  Flags dwFlags {};
  DWORD dwFourCC {};
  DWORD dwRGBBitCount {};
  DWORD dwRBitMask {};
  DWORD dwGBitMask {};
  DWORD dwBBitMask {};
  DWORD dwABitMask {};
};
constexpr bool supports_bitflags(PixelFormat::Flags) { return true; }

// DDS_HEADER
struct Header {
  enum class Flags : DWORD {
    Caps = 1,
    Height = 2,
    Width = 4,
    Pitch = 8,
    PixelFormat = 0x1000,
    MipMapCount = 0x20000,
    LinearSize = 0x80000,
    Depth = 0x800000,
  };
  enum class Caps : DWORD {
    Complex = 0x8,
    MipMap = 0x400000,
    Texture = 0x1000,
  };
  DWORD dwSize {sizeof(Header)};
  Flags dwFlags;
  DWORD dwHeight;
  DWORD dwWidth;
  DWORD dwPitchOrLinearSize;
  DWORD dwDepth;
  DWORD dwMipMapCount;
  DWORD dwReserved1[11];
  PixelFormat ddspf;
  Caps dwCaps;
  DWORD dwCaps2;
  DWORD dwCaps3;
  DWORD dwCaps4;
  DWORD dwReserved2;
};

constexpr bool supports_bitflags(Header::Flags) { return true; }

constexpr bool supports_bitflags(Header::Caps) { return true; }

}// namespace OpenKneeboard::DDS
