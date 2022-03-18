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

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSRadioLogTab::DCSRadioLogTab(const DXResources& dxr)
  : TabWithDoodles(dxr), TabWithPlainTextContent(dxr) {
}

DCSRadioLogTab::~DCSRadioLogTab() {
}

utf8_string DCSRadioLogTab::GetTitle() const {
  return _("Radio Log");
}

uint16_t DCSRadioLogTab::GetPageCount() const {
  const auto count = TabWithPlainTextContent::GetPageCount();
  // We display a placeholder message
  return count == 0 ? 1 : count;
}

void DCSRadioLogTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  TabWithPlainTextContent::RenderPlainTextContent(ctx, pageIndex, rect);
}

const char* DCSRadioLogTab::GetGameEventName() const {
  return DCS::EVT_RADIO_MESSAGE;
}

void DCSRadioLogTab::Update(
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath,
  utf8_string_view value) {
  this->ClearContentCache();
  this->PushMessage(value);
}

void DCSRadioLogTab::OnSimulationStart() {
  this->PushFullWidthSeparator();
}

utf8_string DCSRadioLogTab::GetPlaceholderText() const {
  return _("[waiting for radio messages]");
}

void DCSRadioLogTab::Reload() {
  this->ClearText();
}

}// namespace OpenKneeboard
