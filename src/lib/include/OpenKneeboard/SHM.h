/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <Unknwn.h>
#include <winrt/base.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "bitflags.h"

namespace OpenKneeboard::SHM {

struct Header;

static constexpr bool SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8 = true;
std::wstring SharedTextureName();

#pragma pack(push)
struct VRConfig {
  static constexpr uint16_t VERSION = 1;

  enum class Flags : uint32_t {
    HEADLOCKED = 1 << 0,
    DISCARD_DEPTH_INFORMATION = 1 << 1,
    PREFER_ROOMSCALE_POSITION = 1 << 2,
  };

  // Distances in meters, rotations in radians
  float x = 0.15f, floorY = 0.6f, eyeY = -0.7f, z = -0.4f;
  float rx = -2 * std::numbers::pi_v<float> / 5,
        ry = -std::numbers::pi_v<float> / 32, rz = 0.0f;
  float height = 0.25f;
  float zoomScale = 2.0f;

  Flags flags = Flags::DISCARD_DEPTH_INFORMATION;
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

  uint16_t imageWidth, imageHeight;// Pixels
  VRConfig vr;
  FlatConfig flat;
};

#pragma pack(pop)

class Impl;

class Writer final {
 public:
  Writer();
  ~Writer();

  operator bool() const;
  void Update(const Config& config);

  UINT GetPreviousTextureKey() const;
  UINT GetNextTextureKey() const;

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

class SharedTexture final {
  public:
    SharedTexture();
    SharedTexture(const Header& header, ID3D11Device* d3d);
    ~SharedTexture();

    operator bool() const;
    ID3D11Texture2D* GetTexture() const;
    IDXGISurface* GetSurface() const;

    SharedTexture(const SharedTexture&) = delete;
    SharedTexture(SharedTexture&&) = delete;
  private:
    UINT mKey = 0;
    winrt::com_ptr<ID3D11Texture2D> mTexture;
};

class Snapshot final {
 private:
  std::shared_ptr<std::vector<std::byte>> mBytes;

 public:
  Snapshot();
  Snapshot(std::vector<std::byte>&& bytes);

  uint32_t GetSequenceNumber() const;
  UINT GetTextureKey() const;
  SharedTexture GetSharedTexture(ID3D11Device*) const;
  const Config* const GetConfig() const;

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
template <>
constexpr bool is_bitflags_v<SHM::VRConfig::Flags> = true;
}
