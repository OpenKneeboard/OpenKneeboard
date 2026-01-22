// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <array>
#include <cinttypes>

namespace OpenKneeboard::Shaders::SpriteBatch {

constexpr uint8_t MaxSpritesPerBatch = 16;
constexpr uint8_t VerticesPerSprite = 8;
constexpr uint8_t MaxVerticesPerBatch = VerticesPerSprite * MaxSpritesPerBatch;

struct UniformBuffer {
  using SourceClamp = std::array<float, 4>;
  using SourceDimensions = std::array<float, 2>;

  std::array<SourceClamp, MaxSpritesPerBatch> mSourceClamp;
  std::array<SourceDimensions, MaxSpritesPerBatch> mSourceDimensions;
  std::array<float, 2> mTargetDimensions;
};
// These offsets are included in both the SPIR-V and DXIL generated headers
// Check they match if you change anything
static_assert(offsetof(UniformBuffer, mSourceClamp) == 0);
static_assert(offsetof(UniformBuffer, mSourceDimensions) == 256);
static_assert(offsetof(UniformBuffer, mTargetDimensions) == 384);

struct Vertex {
  class Position : public std::array<float, 4> {
   public:
    constexpr Position() = default;
    constexpr Position(const std::array<float, 2>& pos_2d)
      : std::array<float, 4> {pos_2d[0], pos_2d[0], 0, 1} {}

    constexpr Position(float x, float y) : std::array<float, 4> {x, y, 0, 1} {}
  };
  Position mPosition;
  std::array<float, 4> mColor {};
  std::array<float, 2> mTexCoord;
  uint32_t mTextureIndex {~(0ui32)};
};

}// namespace OpenKneeboard::Shaders::SpriteBatch
