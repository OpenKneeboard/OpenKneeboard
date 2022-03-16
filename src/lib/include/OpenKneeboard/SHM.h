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
#include <shims/winrt.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "FlatConfig.h"
#include "VRConfig.h"

namespace OpenKneeboard::SHM {

struct Header;

static constexpr bool SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8 = true;
std::wstring SharedTextureName(uint32_t sequenceNumber);

#pragma pack(push)
struct Config final {
  static constexpr uint16_t VERSION = 1;

  HWND feederWindow {0};
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
  UINT GetNextTextureIndex() const;

  uint32_t GetNextSequenceNumber() const;

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
  std::unique_ptr<Header> mHeader;

 public:
  Snapshot();
  Snapshot(const Header& header);
  ~Snapshot();

  uint32_t GetSequenceNumber() const;
  SharedTexture GetSharedTexture(ID3D11Device*) const;
  Config GetConfig() const;

  operator bool() const;
};

class Reader final {
 public:
  Reader();
  ~Reader();

  operator bool() const;
  Snapshot MaybeGet() const;
  uint32_t GetSequenceNumber() const;

 private:
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard::SHM
