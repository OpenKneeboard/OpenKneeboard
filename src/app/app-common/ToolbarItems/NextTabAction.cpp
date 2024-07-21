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
#include <OpenKneeboard/AppSettings.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/NextTabAction.h>

namespace OpenKneeboard {

NextTabAction::NextTabAction(
  KneeboardState* kneeboardState,
  const std::shared_ptr<KneeboardView>& kneeboardView)
  : ToolbarAction("\uE74B", _("Next Tab")),
    mKneeboardState(kneeboardState),
    mKneeboardView(kneeboardView) {
  AddEventListener(
    kneeboardView->evCurrentTabChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    kneeboardState->evSettingsChangedEvent, this->evStateChangedEvent);
}

NextTabAction::~NextTabAction() {
  this->RemoveAllEventListeners();
}

bool NextTabAction::IsEnabled() const {
  const auto count = mKneeboardState->GetTabsList()->GetTabs().size();

  if (count < 2) {
    return false;
  }

  if (mKneeboardState->GetAppSettings().mLoopTabs) {
    return true;
  }

  auto kbv = mKneeboardView.lock();
  return kbv && ((kbv->GetTabIndex() + 1) < count);
}

winrt::Windows::Foundation::IAsyncAction NextTabAction::Execute() {
  auto kbv = mKneeboardView.lock();
  if (kbv) {
    kbv->NextTab();
  }
  co_return;
}

}// namespace OpenKneeboard
