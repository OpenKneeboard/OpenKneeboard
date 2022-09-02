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
#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/PlainTextPageSource.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSRadioLogTab::DCSRadioLogTab(const DXResources& dxr, KneeboardState* kbs)
  : PageSourceWithDelegates(dxr, kbs),
    mPageSource(std::make_shared<PlainTextPageSource>(
      dxr,
      _("[waiting for radio messages]"))) {
  this->SetDelegates({mPageSource});
  AddEventListener(mPageSource->evPageAppendedEvent, this->evPageAppendedEvent);
}

DCSRadioLogTab::~DCSRadioLogTab() {
  this->RemoveAllEventListeners();
}

utf8_string DCSRadioLogTab::GetGlyph() const {
  return "\uF12E";
}

utf8_string DCSRadioLogTab::GetTitle() const {
  return _("Radio Log");
}

uint16_t DCSRadioLogTab::GetPageCount() const {
  const auto count = mPageSource->GetPageCount();
  // We display a placeholder message
  return count == 0 ? 1 : count;
}

void DCSRadioLogTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath) {
  if (event.name == DCS::EVT_SIMULATION_START) {
    mPageSource->PushFullWidthSeparator();
    this->evNeedsRepaintEvent.Emit();
    return;
  }

  if (event.name != DCS::EVT_RADIO_MESSAGE) {
    return;
  }

  mPageSource->PushMessage(event.value);
}

void DCSRadioLogTab::Reload() {
  mPageSource->ClearText();
  this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
}

}// namespace OpenKneeboard
