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
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/SwitchProfileAction.h>
#include <OpenKneeboard/SwitchProfileFlyout.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabsList.h>

namespace OpenKneeboard {

SwitchProfileFlyout::SwitchProfileFlyout(KneeboardState* kbs)
  : mKneeboardState(kbs) {
  this->AddEventListener(
    mKneeboardState->evProfileSettingsChangedEvent, this->evStateChangedEvent);
}

SwitchProfileFlyout::~SwitchProfileFlyout() {
  this->RemoveAllEventListeners();
}

std::string_view SwitchProfileFlyout::GetGlyph() const {
  return {};
}
std::string_view SwitchProfileFlyout::GetLabel() const {
  return _("Switch profile");
}

bool SwitchProfileFlyout::IsEnabled() const {
  return true;
}

bool SwitchProfileFlyout::IsVisible() const {
  return mKneeboardState->GetProfileSettings().mEnabled;
}

std::vector<std::shared_ptr<IToolbarItem>> SwitchProfileFlyout::GetSubItems()
  const {
  std::vector<std::shared_ptr<IToolbarItem>> ret;
  for (const auto& profile:
       mKneeboardState->GetProfileSettings().GetSortedProfiles()) {
    ret.push_back(std::make_shared<SwitchProfileAction>(
      mKneeboardState, profile.mID, profile.mName));
  }
  return ret;
}

}// namespace OpenKneeboard
