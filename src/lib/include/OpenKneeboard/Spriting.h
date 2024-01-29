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

#include <OpenKneeboard/Pixels.h>

#include <OpenKneeboard/config.h>

#include <d3d11.h>

namespace OpenKneeboard::Spriting {

namespace Detail {

constexpr uint8_t GetColumnCount(uint8_t maxSprites) {
  return (maxSprites < 4) ? maxSprites : 4;
}

constexpr uint8_t GetRowCount(uint8_t maxSprites) {
  return ((maxSprites - 1) / GetColumnCount(maxSprites)) + 1;
}

static_assert(GetRowCount(1) == 1);
static_assert(GetColumnCount(1) == 1);
static_assert(GetRowCount(4) == 1);
static_assert(GetColumnCount(4) == 4);
static_assert(GetRowCount(5) == 2);
static_assert(GetColumnCount(5) == 4);
static_assert(GetRowCount(8) == 2);
static_assert(GetColumnCount(8) == 4);
}// namespace Detail

constexpr PixelSize GetBufferSize(uint8_t maxSprites) noexcept {
  return {
    MaxViewRenderSize.mWidth * Detail::GetColumnCount(maxSprites),
    MaxViewRenderSize.mHeight * Detail::GetRowCount(maxSprites),
  };
}

static_assert(
  GetBufferSize(MaxViewCount).mWidth <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION);
static_assert(
  GetBufferSize(MaxViewCount).mHeight <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION);

constexpr PixelPoint GetOffset(uint8_t sprite, uint8_t maxSprites) {
  const auto row = sprite / Detail::GetColumnCount(maxSprites);
  const auto column = sprite % Detail::GetColumnCount(maxSprites);
  return {MaxViewRenderSize.mWidth * column, MaxViewRenderSize.mHeight * row};
}

constexpr PixelRect GetRect(uint8_t sprite, uint8_t maxSprites) noexcept {
  return {
    GetOffset(sprite, maxSprites),
    MaxViewRenderSize,
  };
}

}// namespace OpenKneeboard::Spriting