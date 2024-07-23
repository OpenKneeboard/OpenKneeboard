#include <OpenKneeboard/DoodleSettings.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  DoodleSettings::Tool,
  mMinimumRadius,
  mSensitivity)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(DoodleSettings, mPen, mEraser)

}// namespace OpenKneeboard
