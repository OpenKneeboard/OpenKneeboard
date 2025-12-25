// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/Settings.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>

#include <OpenKneeboard/bindline.hpp>
#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/version.hpp>

#include <chrono>
#include <format>

namespace OpenKneeboard {

static auto& Store() {
  static std::weak_ptr<TroubleshootingStore> value;
  return value;
}

std::shared_ptr<TroubleshootingStore> TroubleshootingStore::Get() {
  auto shared = Store().lock();
  if (!shared) {
    shared.reset(new TroubleshootingStore());
    Store() = shared;

    dprint.SetHistoryProvider([weak = std::weak_ptr {shared}]() {
      return weak.lock()->GetDPrintDebugLogAsString();
    });
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

  dprint("{}()", __FUNCTION__);
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

  const auto directory = Filesystem::GetLogsDirectory();

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
                  std::chrono::time_point_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now()),
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
    dprint("Deleting stale log file {}", it.string());
    std::filesystem::remove(it);
  }
}

TroubleshootingStore::~TroubleshootingStore() {
  dprint("{}()", __FUNCTION__);
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

template <class C, class T>
static auto ReadableTime(const std::chrono::time_point<C, T>& time) {
  return std::chrono::zoned_time(
    std::chrono::current_zone(),
    std::chrono::time_point_cast<std::chrono::seconds>(time));
}

std::string TroubleshootingStore::GetAPIEventsDebugLogAsString() const {
  const auto entries = mAPIEvents | std::views::values
    | std::views::transform([](const auto& event) {
                         return std::format(
                           "{}:\n"
                           "  Latest value:  '{}'\n"
                           "  First seen:    {}\n"
                           "  Last seen:     {}\n"
                           "  Receive count: {}\n"
                           "  Change count:  {}",
                           event.mName,
                           event.mValue,
                           ReadableTime(event.mFirstSeen),
                           ReadableTime(event.mLastSeen),
                           event.mReceiveCount,
                           event.mUpdateCount);
                       });
  return std::ranges::fold_left_first(
           entries,
           [](const auto& acc, const auto& it) {
             return std::format("{}\n\n{}", acc, it);
           })
    .value_or(
      std::format(
        "No events as of {}", ReadableTime(std::chrono::system_clock::now())));
}

TroubleshootingStore::DPrintReceiver::~DPrintReceiver() {
}

std::vector<TroubleshootingStore::DPrintEntry>
TroubleshootingStore::DPrintReceiver::GetMessages() {
  std::unique_lock lock(mMutex);
  return mMessages;
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

std::string TroubleshootingStore::GetDPrintDebugLogAsString() const {
  const auto messages = mDPrint->GetMessages();

  if (messages.empty()) {
    return "No log messages (?!)";
  }

  std::wstring ret;
  for (const auto& entry: messages) {
    auto exe = std::wstring_view(entry.mExecutable);
    {
      auto dirSep = exe.find_last_of(L'\\');
      if (dirSep != exe.npos && dirSep + 1 < exe.size()) {
        exe.remove_prefix(dirSep + 1);
      }
    }

    ret += std::format(
      L"[{:%F %T} {} ({})] {}: {}\n",
      std::chrono::zoned_time(
        std::chrono::current_zone(),
        std::chrono::time_point_cast<std::chrono::seconds>(entry.mWhen)),
      exe,
      entry.mProcessID,
      entry.mPrefix,
      entry.mMessage);
  }

  return winrt::to_string(ret);
}

}// namespace OpenKneeboard
