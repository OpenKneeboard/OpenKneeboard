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
  : TabWithDoodles(dxr, kbs),
    mPageSource(std::make_unique<PlainTextPageSource>(
      dxr,
      _("[waiting for radio messages]"))) {
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

D2D1_SIZE_U DCSRadioLogTab::GetNativeContentSize(uint16_t pageIndex) {
  return mPageSource->GetNativeContentSize(pageIndex);
}

void DCSRadioLogTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  mPageSource->RenderPage(ctx, pageIndex, rect);
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

  this->ClearContentCache();
  mPageSource->PushMessage(event.value);
  this->evNeedsRepaintEvent.Emit();
}

void DCSRadioLogTab::Reload() {
  mPageSource->ClearText();
  this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
}

}// namespace OpenKneeboard
