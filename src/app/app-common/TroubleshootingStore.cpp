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
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/TroubleshootingStore.h>

namespace OpenKneeboard {

TroubleshootingStore::TroubleshootingStore() {
}

TroubleshootingStore::~TroubleshootingStore() {
}

void TroubleshootingStore::OnGameEvent(const GameEvent& ev) {
  if (!mGameEvents.contains(ev.name)) {
    mGameEvents[ev.name] = {
      .mFirstSeen = std::chrono::system_clock::now(),
      .mName = ev.name,
    };
  }
  auto& entry = mGameEvents.at(ev.name);

  entry.mLastSeen = std::chrono::system_clock::now();
  entry.mReceiveCount++;
  if (ev.value != entry.mValue) {
    entry.mUpdateCount++;
    entry.mValue = ev.value;
  }

  evGameEventUpdated.Emit(entry);
}

std::vector<TroubleshootingStore::GameEventEntry>
TroubleshootingStore::GetGameEvents() const {
  std::vector<GameEventEntry> events;
  events.reserve(mGameEvents.size());
  for (const auto& [name, event]: mGameEvents) {
    events.push_back(event);
  }
  return events;
}

}// namespace OpenKneeboard
