// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/DoodleSettings.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  DoodleSettings::Tool,
  mMinimumRadius,
  mSensitivity)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(DoodleSettings, mPen, mEraser)

}// namespace OpenKneeboard
