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
#include <OpenKneeboard/AppSettings.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/PreviousTabAction.hpp>
#include <OpenKneeboard/TabsList.hpp>

namespace OpenKneeboard {

PreviousTabAction::PreviousTabAction(
  KneeboardState* kneeboardState,
  const std::shared_ptr<KneeboardView>& kneeboardView)
  : ToolbarAction("\uE74A", _("Previous Tab")),
    mKneeboardState(kneeboardState),
    mKneeboardView(kneeboardView) {
  AddEventListener(
    kneeboardView->evCurrentTabChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    kneeboardState->evSettingsChangedEvent, this->evStateChangedEvent);
}

PreviousTabAction::~PreviousTabAction() {
  this->RemoveAllEventListeners();
}

bool PreviousTabAction::IsEnabled() const {
  auto kbv = mKneeboardView.lock();
  if (!kbv) {
    return false;
  }

  if (kbv->GetTabIndex() > 0) {
    return true;
  }

  if (mKneeboardState->GetTabsList()->GetTabs().size() < 2) {
    return false;
  }

  return mKneeboardState->GetAppSettings().mLoopTabs;
}

winrt::Windows::Foundation::IAsyncAction PreviousTabAction::Execute() {
  if (auto kbv = mKneeboardView.lock()) {
    kbv->PreviousTab();
  }
  co_return;
}

}// namespace OpenKneeboard
