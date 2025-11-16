// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

namespace OpenKneeboard {

enum class ScalingKind {
  /* The source is equivalent to a 2D-array of pixels; scaling is slow and low
   * quality.
   *
   * The native content size should be **strongly** preferred.
   */
  Bitmap,
  /* The source can be rendered at any resolution with high quality, without
   * significiant performance issues.
   */
  Vector,
};

}