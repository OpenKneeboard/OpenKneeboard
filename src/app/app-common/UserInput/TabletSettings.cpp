// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/TabletSettings.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

// Not using sparse json as an individual binding should not be diffed/merged:
// if either the buttons or actions differ, it's a different binding, not a
// modified one.
OPENKNEEBOARD_DEFINE_JSON(TabletSettings::ButtonBinding, mButtons, mAction)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  TabletSettings::Device,
  mID,
  mName,
  mExpressKeyBindings,
  mOrientation)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  TabletSettings,
  mDevices,
  mWarnIfOTDIPCUnusuable)

}// namespace OpenKneeboard
