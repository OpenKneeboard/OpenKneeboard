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
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/Settings.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/version.hpp>

#include <chrono>

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
  DPrintReceiver() = default;
  ~DPrintReceiver();

  std::vector<DPrintEntry> GetMessages();

  Event<DPrintEntry> evMessageReceived;

  DPrintReceiver(const DPrintReceiver&) = delete;
  DPrintReceiver(DPrintReceiver&&) = delete;
  DPrintReceiver& operator=(const DPrintReceiver&) = delete;
  DPrintReceiver& operator=(DPrintReceiver&&) = delete;

 protected:
  void OnMessage(const DPrintMessage& message) override;

 private:
  std::vector<DPrintEntry> mMessages;
  std::recursive_mutex mMutex;
};

TroubleshootingStore::TroubleshootingStore() {
  mDPrint = std::make_unique<DPrintReceiver>();
  mDPrintThread = std::jthread {[this](std::stop_token stopToken) {
    SetThreadDescription(
      GetCurrentThread(), L"TroubleshootingStore DPrintReceiver");
    mDPrint->Run(stopToken);
  }};

  AddEventListener(mDPrint->evMessageReceived, this->evDPrintMessageReceived);

  this->InitializeLogFile();

  dprintf("{}()", __FUNCTION__);
}

void TroubleshootingStore::InitializeLogFile() {
  DWORD maxLogFiles = 0;
  for (auto hkey: {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
    DWORD size = sizeof(maxLogFiles);
    if (
      RegGetValueW(
        hkey,
        RegistrySubKey,
        L"MaxLogFiles",
        RRF_RT_REG_DWORD,
        nullptr,
        &maxLogFiles,
        &size)
      == ERROR_SUCCESS) {
      break;
    }
  }

  if (maxLogFiles == 0) {
    return;
  }

  const auto directory = Filesystem::GetSettingsDirectory() / "logs";
  std::filesystem::create_directories(directory);

  std::vector<std::filesystem::path> existingFiles;
  for (const auto& it: std::filesystem::directory_iterator(directory)) {
    if (!it.is_regular_file()) {
      continue;
    }
    existingFiles.push_back(it.path());
  }
  std::sort(existingFiles.begin(), existingFiles.end());

  const auto file = directory
    / std::format(L"OpenKneeboard-{:%Y%m%dT%H%M%S}-{}.{}.{}.{}-{}.log",
                  std::chrono::system_clock::now(),
                  Version::Major,
                  Version::Minor,
                  Version::Patch,
                  Version::Build,
                  GetCurrentProcessId());
  mLogFile = std::ofstream(file, std::ios::binary);

  AddEventListener(
    this->evDPrintMessageReceived,
    std::bind_front(&TroubleshootingStore::WriteDPrintMessageToLogFile, this));

  if (existingFiles.size() < maxLogFiles) {
    return;
  }
  const auto toDelete = std::vector<std::filesystem::path> {
    existingFiles.begin(),
    existingFiles.begin() + (existingFiles.size() + 1 - maxLogFiles)};
  for (const auto& it: toDelete) {
    dprintf("Deleting stale log file {}", it.string());
    std::filesystem::remove(it);
  }
}

TroubleshootingStore::~TroubleshootingStore() {
  dprintf("{}()", __FUNCTION__);
  this->RemoveAllEventListeners();
}

void TroubleshootingStore::WriteDPrintMessageToLogFile(
  const DPrintEntry& entry) {
  if (!mLogFile) {
    return;
  }

  *mLogFile << std::format(
    "[{:%F %T} {} ({})] {}: {}",
    std::chrono::zoned_time(
      std::chrono::current_zone(),
      std::chrono::time_point_cast<std::chrono::seconds>(entry.mWhen)),
    std::filesystem::path(entry.mExecutable).filename().string(),
    entry.mProcessID,
    winrt::to_string(entry.mPrefix),
    winrt::to_string(entry.mMessage))
            << std::endl;
}

void TroubleshootingStore::OnAPIEvent(const APIEvent& ev) {
  if (!mAPIEvents.contains(ev.name)) {
    mAPIEvents[ev.name] = {
      .mFirstSeen = std::chrono::system_clock::now(),
      .mName = ev.name,
    };
  }
  auto& entry = mAPIEvents.at(ev.name);

  entry.mLastSeen = std::chrono::system_clock::now();
  entry.mReceiveCount++;
  if (ev.value != entry.mValue) {
    entry.mUpdateCount++;
    entry.mValue = ev.value;
  }

  evAPIEventReceived.Emit(entry);
}

std::vector<TroubleshootingStore::APIEventEntry>
TroubleshootingStore::GetAPIEvents() const {
  std::vector<APIEventEntry> events;
  events.reserve(mAPIEvents.size());
  for (const auto& [name, event]: mAPIEvents) {
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
  const DPrintEntry entry {
    .mWhen = std::chrono::system_clock::now(),
    .mProcessID = message.mHeader.mProcessID,
    .mExecutable = message.mHeader.mExecutable,
    .mPrefix = message.mHeader.mPrefix,
    .mMessage = std::wstring {message.mMessage, message.mMessageLength},
  };
  {
    std::unique_lock lock(mMutex);
    mMessages.push_back(entry);
  }
  evMessageReceived.Emit(entry);
}

}// namespace OpenKneeboard
