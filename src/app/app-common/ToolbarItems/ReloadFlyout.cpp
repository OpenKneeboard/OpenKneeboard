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
#include <OpenKneeboard/ReloadFlyout.h>
#include <OpenKneeboard/ReloadTabAction.h>

namespace OpenKneeboard {

ReloadFlyout::ReloadFlyout(
  KneeboardState* kbs,
  const std::shared_ptr<ITab>& tab)
  : ToolbarFlyout(
    "\ue72c",// Refresh
    "Reload",
    {
      std::make_shared<ReloadTabAction>(kbs, tab),
      std::make_shared<ReloadTabAction>(kbs, ReloadTabAction::ReloadAllTabs),
    }) {
}

}// namespace OpenKneeboard
