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
#include <OpenKneeboard/IKneeboardView.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/SetTabAction.h>
#include <OpenKneeboard/SetTabFlyout.h>
#include <OpenKneeboard/TabsList.h>

namespace OpenKneeboard {

SetTabFlyout::SetTabFlyout(
  KneeboardState* kbs,
  const std::shared_ptr<IKneeboardView>& kneeboardView)
  : mKneeboardState(kbs), mKneeboardView(kneeboardView) {
  // FIXME: update when tabs changed
}

SetTabFlyout::~SetTabFlyout() = default;

std::string_view SetTabFlyout::GetGlyph() const {
  return {};
}
std::string_view SetTabFlyout::GetLabel() const {
  return _("Switch Tab");
}

bool SetTabFlyout::IsEnabled() const {
  return true;
}

std::vector<std::shared_ptr<IToolbarItem>> SetTabFlyout::GetSubItems() const {
  std::vector<std::shared_ptr<IToolbarItem>> ret;
  for (const auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
    ret.push_back(std::make_shared<SetTabAction>(mKneeboardView, tab));
  }
  return ret;
}

}// namespace OpenKneeboard
