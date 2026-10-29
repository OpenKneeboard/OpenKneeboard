// OpenKneeboard
//
// Copyright (c) 2026 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/SpriteAtlas.hpp>

#include <RectangleBinPack/MaxRectsBinPack.h>

#include <algorithm>
#include <ranges>

namespace OpenKneeboard {

SpriteAtlas::SpriteAtlas(const std::span<PixelSize>& sprites) {
  // "+1" to allow an empty space between each one
  const auto maxWidth =
    std::ranges::max(sprites, {}, &PixelSize::mWidth).mWidth + 1;
  const auto maxHeight =
    std::ranges::max(sprites, {}, &PixelSize::mHeight).mHeight + 1;
  const auto totalArea = std::ranges::fold_left(
    sprites, uint64_t {0}, [](const uint64_t acc, const PixelSize& it) {
      return acc + ((it.mWidth + 1) * (it.mHeight + 1));
    });
  const auto estimatedEdge = std::max(
    static_cast<uint32_t>(ceil(sqrt(static_cast<double>(totalArea)))), 64ui32);

  uint32_t width = std::bit_ceil(std::max(maxWidth, estimatedEdge));
  uint32_t height = std::bit_ceil(std::max(maxHeight, estimatedEdge));

  while (true) {
    rbp::MaxRectsBinPack bp(width, height, /* allow rotation = */ false);
    mSprites.clear();
    bool failed = false;
    for (auto&& it: sprites) {
      const auto dest = bp.Insert(
        static_cast<int>(it.mWidth + 1),
        static_cast<int>(it.mHeight + 1),
        rbp::MaxRectsBinPack::RectBestShortSideFit);
      if (!(dest.width && dest.height)) {
        failed = true;
        break;
      }
      mSprites.emplace_back(
        PixelPoint {
          static_cast<uint32_t>(dest.x), static_cast<uint32_t>(dest.y)},
        PixelSize {
          static_cast<uint32_t>(dest.width - 1),
          static_cast<uint32_t>(dest.height - 1)});
    }

    if (!failed) {
      break;
    }
    static constexpr auto Increment = 256;
    if (width <= height) {
      width += Increment;
    } else {
      height += Increment;
    }
    static constexpr auto MaxDimension = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    if (width > MaxDimension) {
      width -= Increment;
      height += Increment;
    }
    if (height > MaxDimension) {
      height -= Increment;
      width += Increment;
      if (width > MaxDimension) {
        throw std::runtime_error("Can not fit SpriteAtlas in resource limits");
      }
    }
  }

  OPENKNEEBOARD_ASSERT(mSprites.size() == std::ranges::size(sprites));

  mAtlasSize = {
    std::ranges::max(mSprites, {}, &PixelRect::Right<uint32_t>)
      .Right<uint32_t>(),
    std::ranges::max(mSprites, {}, &PixelRect::Bottom<uint32_t>)
      .Bottom<uint32_t>(),
  };
}

}// namespace OpenKneeboard
