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

#include <chrono>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

NLOHMANN_JSON_SERIALIZE_ENUM(
  DCSRadioLogTab::MissionStartBehavior,
  {
    {DCSRadioLogTab::MissionStartBehavior::DrawHorizontalLine,
     "DrawHorizontalLine"},
    {DCSRadioLogTab::MissionStartBehavior::ClearHistory, "ClearHistory"},
  });

DCSRadioLogTab::DCSRadioLogTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : DCSRadioLogTab(dxr, kbs, {}, _("Radio Log"), {}) {
}

DCSRadioLogTab::DCSRadioLogTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& config)
  : TabBase(persistentID, title),
    DCSTab(kbs),
    PageSourceWithDelegates(dxr, kbs),
    mPageSource(std::make_shared<PlainTextPageSource>(
      dxr,
      kbs,
      _("[waiting for radio messages]"))) {
  this->SetDelegates({mPageSource});
  AddEventListener(mPageSource->evPageAppendedEvent, this->evPageAppendedEvent);
  LoadSettings(config);
}

DCSRadioLogTab::~DCSRadioLogTab() {
  this->RemoveAllEventListeners();
}

void DCSRadioLogTab::LoadSettings(const nlohmann::json& json) {
  if (json.is_null()) {
    return;
  }
  if (json.contains("MissionStartBehavior")) {
    mMissionStartBehavior = json.at("MissionStartBehavior");
  }
  if (json.contains("ShowTimestamps")) {
    mShowTimestamps = json.at("ShowTimestamps");
  }
}

nlohmann::json DCSRadioLogTab::GetSettings() const {
  return {
    {"MissionStartBehavior", mMissionStartBehavior},
    {"ShowTimestamps", mShowTimestamps},
  };
};

DCSRadioLogTab::MissionStartBehavior DCSRadioLogTab::GetMissionStartBehavior()
  const {
  return mMissionStartBehavior;
}

void DCSRadioLogTab::SetMissionStartBehavior(MissionStartBehavior value) {
  mMissionStartBehavior = value;
  this->evSettingsChangedEvent.Emit();
}

bool DCSRadioLogTab::GetTimestampsEnabled() const {
  return this->mShowTimestamps;
}

void DCSRadioLogTab::SetTimestampsEnabled(bool value) {
  this->mShowTimestamps = value;
  this->evSettingsChangedEvent.Emit();
}

std::string DCSRadioLogTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string DCSRadioLogTab::GetStaticGlyph() {
  return "\uF12E";
}

PageIndex DCSRadioLogTab::GetPageCount() const {
  const auto count = mPageSource->GetPageCount();
  // We display a placeholder message
  return count == 0 ? 1 : count;
}

void DCSRadioLogTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath) {
  this->OnGameEventImpl(event, installPath, savedGamesPath);
}

winrt::fire_and_forget DCSRadioLogTab::OnGameEventImpl(
  GameEvent event,
  std::filesystem::path installPath,
  std::filesystem::path savedGamesPath) {
  if (event.name == DCS::EVT_SIMULATION_START) {
    co_await mUIThread;
    {
      const EventDelay eventDelay;
      switch (mMissionStartBehavior) {
        case MissionStartBehavior::DrawHorizontalLine:
          mPageSource->PushFullWidthSeparator();
          break;
        case MissionStartBehavior::ClearHistory:
          mPageSource->ClearText();
          break;
      }
      const auto parsed = event.ParsedValue<DCS::SimulationStartEvent>();
      mPageSource->PushMessage(std::format(
        _(">> Mission started at {:%T}"),
        std::chrono::utc_seconds {
          std::chrono::seconds {parsed.missionStartTime}}));

      this->evNeedsRepaintEvent.Emit();
    }
    co_return;
  }

  if (event.name != DCS::EVT_MESSAGE) {
    co_return;
  }

  const auto parsed = event.ParsedValue<DCS::MessageEvent>();

  co_await mUIThread;

  const EventDelay delay;
  std::string formatted;
  if (mShowTimestamps) {
    formatted = std::format(
      "{:%T} ",
      std::chrono::utc_seconds {std::chrono::seconds {parsed.missionTime}});
  }
  switch (parsed.messageType) {
    case DCS::MessageType::Radio:
      break;
    case DCS::MessageType::Show:
      formatted += "[show] ";
      break;
    case DCS::MessageType::Trigger:
      formatted += ">> ";
      break;
  }
  formatted += parsed.message;
  mPageSource->PushMessage(formatted);
}

void DCSRadioLogTab::Reload() {
  mPageSource->ClearText();
  this->evContentChangedEvent.Emit();
}

}// namespace OpenKneeboard
