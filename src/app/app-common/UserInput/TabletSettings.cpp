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
#include <OpenKneeboard/TabletSettings.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

NLOHMANN_JSON_SERIALIZE_ENUM(
  WintabMode,
  {
    {WintabMode::Disabled, "Disabled"},
    {WintabMode::Enabled, "Enabled"},
    {WintabMode::EnabledInvasive, "EnabledInvasive"},
  })

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
  mWintab,
  mOTDIPC,
  mWarnIfOTDIPCUnusuable)

}// namespace OpenKneeboard
