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
#include <OpenKneeboard/FlatConfig.h>

namespace OpenKneeboard {

PixelRect NonVRConstrainedPosition::Layout(
  PixelSize canvasSize,
  PixelSize imageSize) const {
  const auto aspectRatio
    = static_cast<float>(imageSize.mWidth) / imageSize.mHeight;
  const auto renderHeight = (canvasSize.mHeight * mHeightPercent) / 100;
  const auto renderWidth = static_cast<uint32_t>(renderHeight * aspectRatio);

  using Alignment::Horizontal;
  using Alignment::Vertical;

  uint32_t left = mPaddingPixels;
  switch (mHorizontalAlignment) {
    case Horizontal::Left:
      break;
    case Horizontal::Center:
      left = (canvasSize.mWidth - renderWidth) / 2;
      break;
    case Horizontal::Right:
      left = canvasSize.mWidth - (renderWidth + mPaddingPixels);
      break;
  }

  uint32_t top = mPaddingPixels;
  switch (mVerticalAlignment) {
    case Vertical::Top:
      break;
    case Vertical::Middle:
      top = (canvasSize.mHeight - renderHeight) / 2;
      break;
    case Vertical::Bottom:
      top = canvasSize.mHeight - (renderHeight + mPaddingPixels);
      break;
  }

  return PixelRect {
    .mOffset = {left, top},
    .mSize = {renderWidth, renderHeight},
  };
}

}// namespace OpenKneeboard