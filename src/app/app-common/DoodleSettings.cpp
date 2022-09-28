#include <OpenKneeboard/DoodleSettings.h>
#include <OpenKneeboard/json.h>

namespace OpenKneeboard {
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  DoodleSettings::Tool,
  SkipFirstLowerNext,
  mMinimumRadius,
  mSensitivity)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  DoodleSettings,
  SkipFirstLowerNext,
  mPen,
  mEraser)

}// namespace OpenKneeboard
