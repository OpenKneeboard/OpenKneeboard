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
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/SwitchProfileAction.hpp>

namespace OpenKneeboard {

SwitchProfileAction::SwitchProfileAction(
  KneeboardState* kbs,
  const std::string& profileID,
  const std::string& profileName)
  : ToolbarAction({}, profileName),
    mKneeboardState(kbs),
    mProfileID(profileID) {
}

SwitchProfileAction::~SwitchProfileAction() {
  this->RemoveAllEventListeners();
}

bool SwitchProfileAction::IsChecked() const {
  return mKneeboardState->GetProfileSettings().mActiveProfile == mProfileID;
}

bool SwitchProfileAction::IsEnabled() const {
  return true;
}

winrt::Windows::Foundation::IAsyncAction SwitchProfileAction::Execute() {
  auto profileSettings = mKneeboardState->GetProfileSettings();
  profileSettings.mActiveProfile = mProfileID;
  co_await mKneeboardState->SetProfileSettings(profileSettings);
  co_return;
}

}// namespace OpenKneeboard
