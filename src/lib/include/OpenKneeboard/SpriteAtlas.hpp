// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Pixels.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/fatal.hpp>

#include <algorithm>

namespace OpenKneeboard {

namespace SpritingDetail {

constexpr uint8_t GetColumnCount(const uint8_t maxSprites) {
  OPENKNEEBOARD_ASSERT(maxSprites > 0);
  OPENKNEEBOARD_ASSERT(maxSprites <= Config::MaxViewCount);
  return std::clamp(maxSprites, 1ui8, 4ui8);
}

constexpr uint8_t GetRowCount(const uint8_t maxSprites) {
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
}// namespace SpritingDetail

struct SpriteAtlas {
  SpriteAtlas() = delete;
  template <class R>
    requires std::ranges::sized_range<std::decay_t<R>>
    && std::same_as<std::ranges::range_value_t<std::decay_t<R>>, PixelSize>
  explicit SpriteAtlas(R&& sprites) noexcept {
    using namespace SpritingDetail;

    const auto maxSprites = std::ranges::size(sprites);
    const auto columns = GetColumnCount(maxSprites);
    const auto rows = GetRowCount(maxSprites);
    mAtlasSize = {
      MaxViewRenderSize.mWidth * columns,
      MaxViewRenderSize.mHeight * rows,
    };
    mSprites.reserve(maxSprites);
    for (uint8_t i = 0; i < maxSprites; ++i) {
      mSprites.emplace_back(
        PixelPoint {
          static_cast<uint32_t>(i % columns) * MaxViewRenderSize.mWidth,
          static_cast<uint32_t>(i / columns) * MaxViewRenderSize.mHeight,
        },
        MaxViewRenderSize);
    }
  }

  [[nodiscard]]
  PixelSize GetAtlasSize() const {
    return mAtlasSize;
  }

  [[nodiscard]]
  PixelRect GetRect(const uint8_t spriteIndex) const {
    return mSprites.at(spriteIndex);
  }

  [[nodiscard]]
  PixelPoint GetOffset(const uint8_t spriteIndex) const {
    return mSprites.at(spriteIndex).mOffset;
  }

 private:
  PixelSize mAtlasSize;
  std::vector<PixelRect> mSprites;
};

}// namespace OpenKneeboard
