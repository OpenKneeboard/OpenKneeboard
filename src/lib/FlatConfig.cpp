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

PixelRect FlatConfig::Layout(PixelSize canvasSize, PixelSize imageSize) const {
  const auto aspectRatio
    = static_cast<float>(imageSize.width) / imageSize.height;
  const auto renderHeight = (canvasSize.height * mHeightPercent) / 100;
  const auto renderWidth = static_cast<uint32_t>(renderHeight * aspectRatio);

  uint32_t left = mPaddingPixels;
  switch (mHorizontalAlignment) {
    case HorizontalAlignment::Left:
      break;
    case HorizontalAlignment::Center:
      left = (canvasSize.width - renderWidth) / 2;
      break;
    case HorizontalAlignment::Right:
      left = canvasSize.width - (renderWidth + mPaddingPixels);
      break;
  }

  uint32_t top = mPaddingPixels;
  switch (mVerticalAlignment) {
    case VerticalAlignment::Top:
      break;
    case VerticalAlignment::Middle:
      top = (canvasSize.height - renderHeight) / 2;
      break;
    case VerticalAlignment::Bottom:
      top = canvasSize.height - (renderHeight + mPaddingPixels);
      break;
  }

  return PixelRect {
    .mOrigin = {left, top},
    .mSize = {renderWidth, renderHeight},
  };
}

}