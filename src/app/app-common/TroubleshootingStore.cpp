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
#include <OpenKneeboard/dprint.h>

namespace OpenKneeboard {

static std::weak_ptr<TroubleshootingStore> gStore;

std::shared_ptr<TroubleshootingStore> TroubleshootingStore::Get() {
  auto shared = gStore.lock();
  if (!shared) {
    shared.reset(new TroubleshootingStore());
    gStore = shared;
  }
  return shared;
}

class TroubleshootingStore::DPrintReceiver final
  : public OpenKneeboard::DPrintReceiver {
 public:
  ~DPrintReceiver();

  std::vector<DPrintEntry> GetMessages();

  Event<DPrintEntry> evMessageReceived;

 protected:
  virtual void OnMessage(const DPrintMessage& message) override;

 private:
  std::vector<DPrintEntry> mMessages;
  std::recursive_mutex mMutex;
};

TroubleshootingStore::TroubleshootingStore() {
  mDPrint = std::make_unique<DPrintReceiver>();
  mDPrintThread = {[this](std::stop_token stopToken) {
    SetThreadDescription(
      GetCurrentThread(), L"TroubleshootingStore DPrintReceiver");
    mDPrint->Run(stopToken);
  }};

  AddEventListener(mDPrint->evMessageReceived, this->evDPrintMessageReceived);
}

TroubleshootingStore::~TroubleshootingStore() {
  this->RemoveAllEventListeners();
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

  evGameEventReceived.Emit(entry);
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

TroubleshootingStore::DPrintReceiver::~DPrintReceiver() {
}

std::vector<TroubleshootingStore::DPrintEntry>
TroubleshootingStore::DPrintReceiver::GetMessages() {
  std::unique_lock lock(mMutex);
  return mMessages;
}

std::vector<TroubleshootingStore::DPrintEntry>
TroubleshootingStore::GetDPrintMessages() const {
  return mDPrint->GetMessages();
}

void TroubleshootingStore::DPrintReceiver::OnMessage(
  const DPrintMessage& message) {
  DPrintEntry entry {std::chrono::system_clock::now(), message};
  {
    std::unique_lock lock(mMutex);
    mMessages.push_back(entry);
  }
  evMessageReceived.Emit(entry);
}

}// namespace OpenKneeboard
