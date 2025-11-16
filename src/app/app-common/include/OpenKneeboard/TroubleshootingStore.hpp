// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Events.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace OpenKneeboard {

struct APIEvent;

class TroubleshootingStore final : private EventReceiver {
 public:
  static std::shared_ptr<TroubleshootingStore> Get();
  ~TroubleshootingStore();

  struct APIEventEntry {
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

  void OnAPIEvent(const APIEvent&);

  std::string GetAPIEventsDebugLogAsString() const;
  std::string GetDPrintDebugLogAsString() const;

  Event<APIEventEntry> evAPIEventReceived;
  Event<DPrintEntry> evDPrintMessageReceived;

 private:
  class DPrintReceiver;
  std::unique_ptr<DPrintReceiver> mDPrint;
  std::jthread mDPrintThread;
  std::map<std::string, APIEventEntry> mAPIEvents;
  std::optional<std::ofstream> mLogFile;

  void InitializeLogFile();
  void WriteDPrintMessageToLogFile(const DPrintEntry&);

  TroubleshootingStore();
};

}// namespace OpenKneeboard
