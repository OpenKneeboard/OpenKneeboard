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
#include <OpenKneeboard/SwitchTabAction.h>
#include <OpenKneeboard/SwitchTabFlyout.h>
#include <OpenKneeboard/TabsList.h>

namespace OpenKneeboard {

SwitchTabFlyout::SwitchTabFlyout(
  KneeboardState* kbs,
  const std::shared_ptr<IKneeboardView>& kneeboardView)
  : mKneeboardState(kbs), mKneeboardView(kneeboardView) {
  AddEventListener(
    kbs->GetTabsList()->evTabsChangedEvent, this->evStateChangedEvent);
}

SwitchTabFlyout::~SwitchTabFlyout() {
  RemoveAllEventListeners();
}

std::string_view SwitchTabFlyout::GetGlyph() const {
  return {};
}
std::string_view SwitchTabFlyout::GetLabel() const {
  return _("Switch tab");
}

bool SwitchTabFlyout::IsEnabled() const {
  return true;
}

std::vector<std::shared_ptr<IToolbarItem>> SwitchTabFlyout::GetSubItems()
  const {
  std::vector<std::shared_ptr<IToolbarItem>> ret;
  auto kbv = mKneeboardView.lock();
  if (!kbv) {
    return ret;
  }

  for (const auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
    ret.push_back(std::make_shared<SwitchTabAction>(kbv, tab));
  }
  return ret;
}

}// namespace OpenKneeboard
