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
#pragma once

#include <OpenKneeboard/Events.h>

#include <chrono>
#include <string>
#include <vector>

namespace OpenKneeboard {

struct GameEvent;

class TroubleshootingStore final {
 public:
  TroubleshootingStore();
  ~TroubleshootingStore();

  struct GameEventEntry {
    std::chrono::system_clock::time_point mFirstSeen;
    std::chrono::system_clock::time_point mLastSeen;

    uint64_t mReceiveCount = 0;
    uint64_t mUpdateCount = 0;

    std::string mName;
    std::string mValue;
  };

  void OnGameEvent(const GameEvent&);

  std::vector<GameEventEntry> GetGameEvents() const;

  Event<GameEventEntry> evGameEventUpdated;

 private:
  std::map<std::string, GameEventEntry> mGameEvents;
};

}// namespace OpenKneeboard
