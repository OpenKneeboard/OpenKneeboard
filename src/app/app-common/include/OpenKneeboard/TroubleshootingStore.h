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
#include <OpenKneeboard/dprint.h>

#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace OpenKneeboard {

struct GameEvent;

class TroubleshootingStore final : private EventReceiver {
 public:
  static std::shared_ptr<TroubleshootingStore> Get();
  ~TroubleshootingStore();

  struct GameEventEntry {
    std::chrono::system_clock::time_point mFirstSeen;
    std::chrono::system_clock::time_point mLastSeen;

    uint64_t mReceiveCount = 0;
    uint64_t mUpdateCount = 0;

    std::string mName;
    std::string mValue;
  };

  struct DPrintEntry {
    std::chrono::system_clock::time_point mWhen;
    DWORD mProcessID {};
    std::wstring mExecutable;
    std::wstring mPrefix;
    std::wstring mMessage;
  };

  void OnGameEvent(const GameEvent&);

  std::vector<GameEventEntry> GetGameEvents() const;
  std::vector<DPrintEntry> GetDPrintMessages() const;

  Event<GameEventEntry> evGameEventReceived;
  Event<DPrintEntry> evDPrintMessageReceived;

 private:
  class DPrintReceiver;
  std::unique_ptr<DPrintReceiver> mDPrint;
  std::jthread mDPrintThread;
  std::map<std::string, GameEventEntry> mGameEvents;
  std::optional<std::ofstream> mLogFile;

  void InitializeLogFile();
  void WriteDPrintMessageToLogFile(const DPrintEntry&);

  TroubleshootingStore();
};

}// namespace OpenKneeboard
