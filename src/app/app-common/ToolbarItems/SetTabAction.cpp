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
#include <OpenKneeboard/SetTabAction.h>
#include <OpenKneeboard/TabsList.h>

namespace OpenKneeboard {

SetTabAction::SetTabAction(
  const std::shared_ptr<IKneeboardView>& kneeboardView,
  const std::shared_ptr<ITab>& tab)
  : ToolbarAction({}, tab->GetTitle()),
    mKneeboardView(kneeboardView),
    mTabID(tab->GetRuntimeID()) {
  // FIXME: update state
}

SetTabAction::~SetTabAction() {
  this->RemoveAllEventListeners();
}

bool SetTabAction::IsChecked() const {
  return mKneeboardView->GetCurrentTabView()->GetRootTab()->GetRuntimeID()
    == mTabID;
}

bool SetTabAction::IsEnabled() {
  return true;
}

void SetTabAction::Execute() {
  mKneeboardView->SetCurrentTabByRuntimeID(mTabID);
}

}// namespace OpenKneeboard
