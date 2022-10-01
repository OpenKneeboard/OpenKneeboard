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

NLOHMANN_JSON_SERIALIZE_ENUM(
  DCSRadioLogTab::MissionStartBehavior,
  {
    {DCSRadioLogTab::MissionStartBehavior::DrawHorizontalLine,
     "DrawHorizontalLine"},
    {DCSRadioLogTab::MissionStartBehavior::ClearHistory, "ClearHistory"},
  });

DCSRadioLogTab::DCSRadioLogTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const std::string& /* title */,
  const nlohmann::json& config)
  : PageSourceWithDelegates(dxr, kbs),
    mPageSource(std::make_shared<PlainTextPageSource>(
      dxr,
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
}

nlohmann::json DCSRadioLogTab::GetSettings() const {
  return {
    {"MissionStartBehavior", mMissionStartBehavior},
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

utf8_string DCSRadioLogTab::GetGlyph() const {
  return "\uF12E";
}

utf8_string DCSRadioLogTab::GetTitle() const {
  return _("Radio Log");
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
  const GameEvent& event,
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath) {
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
      this->evNeedsRepaintEvent.Emit();
    }
    co_return;
  }

  if (event.name != DCS::EVT_RADIO_MESSAGE) {
    co_return;
  }

  co_await mUIThread;

  const EventDelay delay;
  mPageSource->PushMessage(event.value);
}

void DCSRadioLogTab::Reload() {
  mPageSource->ClearText();
  this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
}

}// namespace OpenKneeboard
