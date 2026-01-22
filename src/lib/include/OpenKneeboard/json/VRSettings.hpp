// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/json_fwd.hpp>

namespace OpenKneeboard {

struct GazeTargetScale;
struct VROpacitySettings;
struct VRPose;
struct VRSettings;

OPENKNEEBOARD_DECLARE_SPARSE_JSON(GazeTargetScale)
OPENKNEEBOARD_DECLARE_SPARSE_JSON(VROpacitySettings)
OPENKNEEBOARD_DECLARE_SPARSE_JSON(VRPose)
OPENKNEEBOARD_DECLARE_SPARSE_JSON(VRSettings)

}// namespace OpenKneeboard
