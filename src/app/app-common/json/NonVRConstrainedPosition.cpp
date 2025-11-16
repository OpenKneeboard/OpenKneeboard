// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/NonVRConstrainedPosition.hpp>

#include <OpenKneeboard/json/Alignment.hpp>
#include <OpenKneeboard/json/NonVRConstrainedPosition.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  NonVRConstrainedPosition,
  mHeightPercent,
  mPaddingPixels,
  mHorizontalAlignment,
  mVerticalAlignment)

}// namespace OpenKneeboard
