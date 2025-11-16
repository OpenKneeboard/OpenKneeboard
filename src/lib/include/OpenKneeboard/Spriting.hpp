// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Pixels.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/fatal.hpp>

#include <d3d11.h>

#include <algorithm>

namespace OpenKneeboard::Spriting {

namespace Detail {

constexpr uint8_t GetColumnCount(uint8_t maxSprites) {
  OPENKNEEBOARD_ASSERT(maxSprites > 0);
  OPENKNEEBOARD_ASSERT(maxSprites <= Config::MaxViewCount);
  return std::clamp(maxSprites, 1ui8, 4ui8);
}

constexpr uint8_t GetRowCount(uint8_t maxSprites) {
  OPENKNEEBOARD_ASSERT(maxSprites > 0);
  OPENKNEEBOARD_ASSERT(maxSprites <= Config::MaxViewCount);

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