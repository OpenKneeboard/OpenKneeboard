// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/NonVRConstrainedPosition.hpp>

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