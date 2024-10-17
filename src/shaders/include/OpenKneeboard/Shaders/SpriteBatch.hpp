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

#include <array>
#include <cinttypes>

namespace OpenKneeboard::Shaders::SpriteBatch {

constexpr static uint8_t MaxSpritesPerBatch = 16;

struct UniformBuffer {
  template <class T>
  struct alignas(16) HLSLArrayElement : T {};

  using SourceClamp = HLSLArrayElement<std::array<float, 4>>;
  using SourceDimensions = HLSLArrayElement<std::array<float, 2>>;
  std::array<SourceClamp, MaxSpritesPerBatch> mSourceClamp;
  std::array<SourceDimensions, MaxSpritesPerBatch> mSourceDimensions;
  std::array<float, 2> mTargetDimensions;
};

struct Vertex {
  class Position : public std::array<float, 4> {
   public:
    constexpr Position(const std::array<float, 2>& pos_2d)
      : std::array<float, 4> {pos_2d[0], pos_2d[0], 0, 1} {
    }

    constexpr Position(float x, float y) : std::array<float, 4> {x, y, 0, 1} {
    }
  };
  Position mPosition;
  std::array<float, 4> mColor {};
  std::array<float, 2> mTexCoord;
  uint32_t mTextureIndex {~(0ui32)};
};

}// namespace OpenKneeboard::Shaders::SpriteBatch